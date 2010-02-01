#include <QApplication>
#include <QPushButton>
#include <QProgressBar>
#include <QWidget>
#include <QVBoxLayout>
#include <QGridLayout>
#include <QLabel>
#include <QThread>
#include <QTimer>
#include <QTime>

#ifndef UINT32_MAX
 # define UINT32_MAX (4294967295U)
#endif
#include "i7z.h"


bool global_in_i7z_main_thread=false;

class MyThread: public QThread
{
	public:
		unsigned int numCPUs;
		MyThread();					
		void run();
		double FREQ[4],MULT[4];
		long double C0_TIME[4],C1_TIME[4],C3_TIME[4],C6_TIME[4];
};

MyThread::MyThread()
{
  //ask linux how many cpus are enabled
  numCPUs = sysconf(_SC_NPROCESSORS_ONLN);
  int i;
  for(i=0 ;i<numCPUs; i++)
  {
		FREQ[i]=0; MULT[i]=0; C0_TIME[i]=0; C1_TIME[i]=0; C3_TIME[i]=0; C6_TIME[i]=0;
  }
}

//_FREQ[i], _MULT[i],C0_time[i]*100,C1_time[i]*100,C3_time[i]*100,C6_time[i]*100



void MyThread::run()
{
  int width=1;
  int i,j;
  
  //MSR number and hi:low bit of that MSR
  int PLATFORM_INFO_MSR = 206; //CE 15:8
  int PLATFORM_INFO_MSR_low = 8; 
  int PLATFORM_INFO_MSR_high = 15; 
	
  int IA32_MISC_ENABLE = 416;
  int TURBO_FLAG_low = 38;	
  int TURBO_FLAG_high = 38;	
	
  int MSR_TURBO_RATIO_LIMIT=429;	
	
  int CPU_NUM;
  int CPU_Multiplier;	
  float BLCK;	
  char TURBO_MODE;

  printf("modprobbing for msr");
  system("modprobe msr"); sleep(1);

  system("cat /proc/cpuinfo |grep MHz|sed 's/cpu\\sMHz\\s*:\\s//'|tail -n 1 > cpufreq.txt");
  unsigned int num_Logical_OS, num_Logical_process, num_Processor_Core, num_Physical_Socket;
  
  #ifdef USE_INTEL_CPUID
  get_CPUs_info(&num_Logical_OS, &num_Logical_process, &num_Processor_Core, &num_Physical_Socket);
  #endif
  
  int MAX_TURBO_1C=get_msr_value(CPU_NUM,MSR_TURBO_RATIO_LIMIT, 7,0);
  int MAX_TURBO_2C=get_msr_value(CPU_NUM,MSR_TURBO_RATIO_LIMIT, 15,8);
  int MAX_TURBO_3C=get_msr_value(CPU_NUM,MSR_TURBO_RATIO_LIMIT, 23,16);
  int MAX_TURBO_4C=get_msr_value(CPU_NUM,MSR_TURBO_RATIO_LIMIT, 31,24);

  FILE *cpufreq_file;
  cpufreq_file = fopen("cpufreq.txt","r");
  char cpu_freq_str[30];
  fgets(cpu_freq_str, 30, cpufreq_file);
    
  double cpu_freq_cpuinfo = atof(cpu_freq_str);
  char * pat = "%*lld\n";
  
  cpu_freq_cpuinfo = estimate_MHz();
  //mvprintw(3,0,"True Frequency (without accounting Turbo) %f\n",cpu_freq_cpuinfo);

  CPU_NUM=0;
   
  CPU_Multiplier=get_msr_value(CPU_NUM,PLATFORM_INFO_MSR, PLATFORM_INFO_MSR_high,PLATFORM_INFO_MSR_low);    
  BLCK = cpu_freq_cpuinfo/CPU_Multiplier;
 //  mvprintw(4,0,"CPU Multiplier %dx || Bus clock frequency (BCLK) %f MHz \n", CPU_Multiplier, BLCK);
  TURBO_MODE = turbo_status();//get_msr_value(CPU_NUM,IA32_MISC_ENABLE, TURBO_FLAG_high,TURBO_FLAG_low);
  
  float TRUE_CPU_FREQ;   
  if (TURBO_MODE==1){// && (CPU_Multiplier+1)==MAX_TURBO_2C){
//	mvprintw(5,0,"TURBO ENABLED on %d Cores\n",numCPUs);
	TRUE_CPU_FREQ = BLCK*(CPU_Multiplier+1);
  }else{
//	mvprintw(5,0,"TURBO DISABLED on %d Cores\n",numCPUs);
	TRUE_CPU_FREQ = BLCK*(CPU_Multiplier);
  }
	  
   
  int IA32_PERF_GLOBAL_CTRL=911;//3BF
  int IA32_PERF_GLOBAL_CTRL_Value=get_msr_value(CPU_NUM,IA32_PERF_GLOBAL_CTRL, 63,0);
  int IA32_FIXED_CTR_CTL=909; //38D
  int IA32_FIXED_CTR_CTL_Value=get_msr_value(CPU_NUM,IA32_FIXED_CTR_CTL, 63,0);
  
  //printf("IA32_PERF_GLOBAL_CTRL %d\n",IA32_PERF_GLOBAL_CTRL_Value);
  //printf("IA32_FIXED_CTR_CTL %d\n",IA32_FIXED_CTR_CTL_Value);
  
  unsigned long long int CPU_CLK_UNHALTED_CORE, CPU_CLK_UNHALTED_REF, CPU_CLK_C3, CPU_CLK_C6, CPU_CLK_C1;
  
  CPU_CLK_UNHALTED_CORE=get_msr_value(CPU_NUM,778, 63,0);
  CPU_CLK_UNHALTED_REF   =get_msr_value(CPU_NUM,779, 63,0);
  
  unsigned long int old_val_CORE[numCPUs],new_val_CORE[numCPUs];
  unsigned long int old_val_REF[numCPUs],new_val_REF[numCPUs];
  unsigned long int old_val_C3[numCPUs],new_val_C3[numCPUs];
  unsigned long int old_val_C6[numCPUs],new_val_C6[numCPUs];
  unsigned long int old_val_C1[numCPUs],new_val_C1[numCPUs];
  
  unsigned long long int old_TSC[numCPUs], new_TSC[numCPUs];
  
  struct timezone tz;
  struct timeval tvstart[numCPUs], tvstop[numCPUs];
 
  struct timespec one_second_sleep;
  one_second_sleep.tv_sec=0;
  one_second_sleep.tv_nsec=999999999;// 1000msec
  
  
  unsigned long int IA32_MPERF = get_msr_value(CPU_NUM,231, 7,0);
  unsigned long int IA32_APERF = get_msr_value(CPU_NUM,232, 7,0);
//   mvprintw(12,0,"Wait...\n"); refresh();
   nanosleep(&one_second_sleep,NULL);
   IA32_MPERF = get_msr_value(CPU_NUM,231, 7,0) - IA32_MPERF;	 
   IA32_APERF  = get_msr_value(CPU_NUM,232, 7,0) - IA32_APERF;
   
   //printf("Diff. i n APERF = %u, MPERF = %d\n", IA32_MPERF, IA32_APERF);
  
  long double C0_time[numCPUs], C1_time[numCPUs], C3_time[numCPUs], C6_time[numCPUs];
  double _FREQ[numCPUs], _MULT[numCPUs];
    
//  mvprintw(12,0,"Current Freqs\n");
  
  for (i=0;i<numCPUs;i++){
	 CPU_NUM=i;
	IA32_PERF_GLOBAL_CTRL_Value=get_msr_value(CPU_NUM,IA32_PERF_GLOBAL_CTRL, 63,0);
	set_msr_value(CPU_NUM, IA32_PERF_GLOBAL_CTRL,0x700000003);
	IA32_FIXED_CTR_CTL_Value=get_msr_value(CPU_NUM,IA32_FIXED_CTR_CTL, 63,0);
	set_msr_value(CPU_NUM, IA32_FIXED_CTR_CTL,819);	  
	IA32_PERF_GLOBAL_CTRL_Value=get_msr_value(CPU_NUM,IA32_PERF_GLOBAL_CTRL, 63,0);
	IA32_FIXED_CTR_CTL_Value=get_msr_value(CPU_NUM,IA32_FIXED_CTR_CTL, 63,0);

	old_val_CORE[i] = get_msr_value(CPU_NUM,778, 63,0);
	old_val_REF[i]    = get_msr_value(CPU_NUM,779, 63,0);
	old_val_C3[i]	= get_msr_value(CPU_NUM,1020, 63,0); 
	old_val_C6[i]	= get_msr_value(CPU_NUM,1021, 63,0); 
	old_TSC[i] = rdtsc();
 }     
	  
 
 
  for (;;){
	 nanosleep(&one_second_sleep,NULL);
//	  mvprintw(14,0,"\tProcessor  :Actual Freq (Mult.)\tC0\% Halt(C1) \%\t  C3 \%\t C6 \%\n");
		
	  for (i=0;i<numCPUs;i++){
			CPU_NUM=i;
			new_val_CORE[i] = get_msr_value(CPU_NUM,778, 63,0);
			new_val_REF[i]    = get_msr_value(CPU_NUM,779, 63,0);
			new_val_C3[i]	 = get_msr_value(CPU_NUM,1020, 63,0); 
			new_val_C6[i]	 = get_msr_value(CPU_NUM,1021, 63,0); 
			//gettimeofday(&tvstop[i], &tz);
			new_TSC[i] = rdtsc();
			if (old_val_CORE[i]>new_val_CORE[i]){
				 CPU_CLK_UNHALTED_CORE=(3.40282366921e38-old_val_CORE[i]) +new_val_CORE[i];  	
			}else { 
				CPU_CLK_UNHALTED_CORE=new_val_CORE[i] - old_val_CORE[i];	  
			}
		
			//number of TSC cycles while its in halted state
			if ((new_TSC[i] - old_TSC[i]) <  CPU_CLK_UNHALTED_CORE)
				CPU_CLK_C1=0;
			else
				CPU_CLK_C1 = ((new_TSC[i] - old_TSC[i]) - CPU_CLK_UNHALTED_CORE);
		
			if(old_val_REF[i]>new_val_REF[i]){
				CPU_CLK_UNHALTED_REF=(3.40282366921e38-old_val_REF[i]) +new_val_REF[i];
			}else{
				CPU_CLK_UNHALTED_REF=new_val_REF[i] - old_val_REF[i];	   
			}
		
			if(old_val_C3[i]>new_val_C3[i]){
				CPU_CLK_C3=(3.40282366921e38-old_val_C3[i]) +new_val_C3[i];
			}else{
				CPU_CLK_C3=new_val_C3[i] - old_val_C3[i];	   
			}
		
			if(old_val_C6[i]>new_val_C6[i]){
				CPU_CLK_C6=(3.40282366921e38-old_val_C6[i]) +new_val_C6[i];
			}else{
				CPU_CLK_C6=new_val_C6[i] - old_val_C6[i];	   
			}
			
			//microseconds = ((tvstop[i].tv_sec-tvstart[i].tv_sec)*1000000) +	(tvstop[i].tv_usec-tvstart[i].tv_usec);
			//printf("elapsed secs %llu\n",microseconds);
			//printf("%u %u\n",CPU_CLK_UNHALTED_CORE,CPU_CLK_UNHALTED_REF);
			//printf("%llu\n",CPU_CLK_UNHALTED_CORE);
			//CPU_CLK_UNHALTED_REF = CPU_CLK_UNHALTED_REF;
			//CPU_CLK_UNHALTED_CORE = CPU_CLK_UNHALTED_CORE;
			//printf("%llu %llu %llu\n",CPU_CLK_UNHALTED_CORE,CPU_CLK_UNHALTED_REF,(new_TSC[i]-old_TSC[i]));
			_FREQ[i] =   estimate_MHz()*((long double)CPU_CLK_UNHALTED_CORE/(long double)CPU_CLK_UNHALTED_REF);  
			_MULT[i] = _FREQ[i]/BLCK;
		
			C0_time[i]=((long double)CPU_CLK_UNHALTED_REF/(long double)(new_TSC[i]-old_TSC[i]));
			C1_time[i]=((long double)CPU_CLK_C1/(long double)(new_TSC[i]-old_TSC[i]));
			C3_time[i]=((long double)CPU_CLK_C3/(long double)(new_TSC[i]-old_TSC[i]));
			C6_time[i]=((long double)CPU_CLK_C6/(long double)(new_TSC[i]-old_TSC[i]));

		C1_time[i] -= C3_time[i]+C6_time[i];

			if (C0_time[i]<1e-2)
				if (C0_time[i]>1e-4)
					C0_time[i]=0.01;
				else
					C0_time[i]=0;
		
			if (C1_time[i]<1e-2)
				if (C1_time[i]>1e-4)
					C1_time[i]=0.01;
				else
					C1_time[i]=0;
		
			if (C3_time[i]<1e-2)
				if (C3_time[i]>1e-4)
					C3_time[i]=0.01;
				else
					C3_time[i]=0;
		
			if (C6_time[i]<1e-2)
				if (C6_time[i]>1e-4)
					C6_time[i]=0.01;
				else
					C6_time[i]=0;
		
	  }
	  
	TRUE_CPU_FREQ=0;
	for(i=0;i<4;i++)
	    if(_FREQ[i]>  TRUE_CPU_FREQ)
		    TRUE_CPU_FREQ = _FREQ[i];
	   
	memcpy(old_val_CORE ,new_val_CORE,sizeof(unsigned long int)*4);
	memcpy(old_val_REF ,new_val_REF,sizeof(unsigned long int)*4);
	memcpy(old_val_C3 ,new_val_C3,sizeof(unsigned long int)*4);
	memcpy(old_val_C6 ,new_val_C6,sizeof(unsigned long int)*4);
	memcpy(tvstart,tvstop,sizeof(struct timeval)*4);
	memcpy(old_TSC,new_TSC,sizeof(unsigned long long int)*4);
	
	memcpy(FREQ, _FREQ, sizeof(double)*4);
	memcpy(MULT, _MULT, sizeof(double)*4);
	memcpy(C0_TIME, C0_time, sizeof(long double)*4);
	memcpy(C1_TIME, C1_time, sizeof(long double)*4);
	memcpy(C3_TIME, C3_time, sizeof(long double)*4);
	memcpy(C6_TIME, C6_time, sizeof(long double)*4);  
	global_in_i7z_main_thread = true;
	}

}

class MyWidget: public QWidget
{
	Q_OBJECT
	public:
		QProgressBar *C0_l[4], *C1_l[4], *C3_l[4], *C6_l[4];
		QLabel *C0, *C1, *C3, *C6;
		QLabel *Freq_[4];
		QLabel *StatusMessage, * Curr_Freq;
		QLabel *ProcNames[4];
		MyWidget(QWidget *parent = 0);
		MyThread *mythread;

	private slots:
		void UpdateWidget();
};

MyWidget::MyWidget(QWidget *parent)
	: QWidget(parent)
{
	int i, j;
	for (i=0;i<4;i++){
		C0_l[i] = new QProgressBar; C0_l[i]->setMaximum(99);C0_l[i]->setMinimum(0);
		C1_l[i] = new QProgressBar; C1_l[i]->setMaximum(99);C1_l[i]->setMinimum(0);
		C3_l[i] = new QProgressBar; C3_l[i]->setMaximum(99);C3_l[i]->setMinimum(0);
		C6_l[i] = new QProgressBar; C6_l[i]->setMaximum(99);C6_l[i]->setMinimum(0);
		Freq_[i] = new QLabel(tr(""));
	}

	QGridLayout *layout1 = new QGridLayout;
	
	
	for(i=0; i<4; i++){
		layout1-> addWidget(C0_l[i],i+1,1);
		layout1-> addWidget(C1_l[i],i+1,2);
		layout1-> addWidget(C3_l[i],i+1,3);
		layout1-> addWidget(C6_l[i],i+1,4);
	}

	C0 = new QLabel(tr("C0")); C0->setAlignment( Qt::AlignCenter);
	C1 = new QLabel(tr("C1")); C1->setAlignment( Qt::AlignCenter);
	C3 = new QLabel(tr("C3")); C3->setAlignment( Qt::AlignCenter);
	C6 = new QLabel(tr("C6")); C6->setAlignment( Qt::AlignCenter);

	ProcNames[0] = new QLabel(tr("Processor-1"));
	ProcNames[1] = new QLabel(tr("Processor-2"));
	ProcNames[2] = new QLabel(tr("Processor-3"));
	ProcNames[3] = new QLabel(tr("Processor-4"));

	StatusMessage = new QLabel(tr("Wait")); 
	Curr_Freq = new QLabel(tr("Wait")); 

	layout1->addWidget(ProcNames[0],1,0);
	layout1->addWidget(ProcNames[1],2,0);
	layout1->addWidget(ProcNames[2],3,0);
	layout1->addWidget(ProcNames[3],4,0);

	layout1->addWidget(C0,0,1);
	layout1->addWidget(C1,0,2);
	layout1->addWidget(C3,0,3);
	layout1->addWidget(C6,0,4);

	layout1->addWidget(Freq_[0],1,5);
	layout1->addWidget(Freq_[1],2,5);
	layout1->addWidget(Freq_[2],3,5);
	layout1->addWidget(Freq_[3],4,5);

	layout1->addWidget(StatusMessage,5,4);
	layout1->addWidget(Curr_Freq,5,5);

	QTimer *timer = new QTimer(this);
	connect(timer, SIGNAL(timeout()), this, SLOT(UpdateWidget()));
	timer->start(1000);

	mythread = new MyThread();
	mythread->start();

	setLayout(layout1);
}

void MyWidget::UpdateWidget(){
	int i;
	char val2set[100];
	snprintf(val2set,100,"%0.2f Ghz",mythread->FREQ[0]); Freq_[0]->setText(val2set);
	snprintf(val2set,100,"%0.2f Ghz",mythread->FREQ[1]); Freq_[1]->setText(val2set);
	snprintf(val2set,100,"%0.2f Ghz",mythread->FREQ[2]); Freq_[2]->setText(val2set);
	snprintf(val2set,100,"%0.2f Ghz",mythread->FREQ[3]); Freq_[3]->setText(val2set);
	
	for(i=0;i<4;i++){
		C0_l[i]->setValue(mythread->C0_TIME[i]*100);
		C1_l[i]->setValue(mythread->C1_TIME[i]*100);
		C3_l[i]->setValue(mythread->C3_TIME[i]*100);
		C6_l[i]->setValue(mythread->C6_TIME[i]*100);
	}
	float Max_Freq=0;
	for (i=0;i<4;i++){
		if(mythread->FREQ[i]>Max_Freq)
			Max_Freq = mythread->FREQ[i];
	}
	StatusMessage->setText("Current Freq:");
	snprintf(val2set,100,"%0.2f Ghz",Max_Freq);	Curr_Freq->setText(val2set);
}



 int main(int argc, char *argv[])
 {
    QApplication app(argc, argv);

	MyWidget i7z_widget;
	i7z_widget.show();	
	  return app.exec();
 }

#include "GUI_i7z.moc"
