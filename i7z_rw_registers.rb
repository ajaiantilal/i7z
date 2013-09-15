#* ----------------------------------------------------------------------- *
# *
# *   Under GPL v3
# *   written by Abhishek Jaiantilal
# *   much thanks to Antonio Meireles who suggested this idea & helped with testing 
# *   run as      sudo ruby i7z_rw_registers.rb
# *
# * ----------------------------------------------------------------------- */

#!/usr/bin/ruby

def print_command_list()
	print "Do you need help? \n"
	print "Possible commands are \n" 
	print "help : which will print this list again \n"
	print "turbo : which examines the turbo status \n"
	print "multiplier : examines the multipliers \n"
	print "power : which prints current wattage of the system\n"
	print "clock : allows for software clock modulation ( a form of throttling )\n"
    print "system : allows to print some system info\n"
	print "quit : which will quit the program or just do ctrl + c\n"
end

IA32_PERF_STATUS =  0x198  	#read only
IA32_PERF_CTL = 0x199  		#read write
IA32_MISC_ENABLE = 0x1a0

def num_cores 
    numcores = `cat /proc/cpuinfo | grep 'cpu cores' |head -n 1|sed 's/cpu cores\\s*:\\s//'`
    return numcores.to_i
end

def nehalem_or_sandybridge
    proc_type = `cat /proc/cpuinfo | grep flags |head -n 1| grep avx`
    
    if proc_type.chomp.length == 0
        return 'Nehalem'
    else 
        return 'Sandy Bridge'
    end
end

def turbo_command_list()
	print "turbo : which examines the turbo status \n"
	print "   probable commands are\n"
	print	"	turbo status : get the current status of turbo\n"
	print	"	turbo enable : enable turbo\n"
	print	"	turbo disable : disable turbo\n"
end

def multiplier_command_list()
	print "multiplier : which allows setting/examining multiplier \n"
	print "   probable commands are\n"
	print	"	multiplier set <number> : set the multiplier to given number and ONLY in decimals\n"
	print	"	multiplier get : examine the current multiplier\n"
end

def power_command_list()
	print "power : which allows setting/examining TDP / TDC\n"
	print "   probable commands are\n"
	print	"	power set tdp <number> : set the multiplier to given number and ONLY in decimals\n"
	print	"	power set tdc <number> : set the multiplier to given number and ONLY in decimals\n"
	print	"	power get : examine the current TDP / TDC\n"
end

def clock_command_list()
	print "clock : allows for software clock modulation ( a form of throttling )\n"
	print " a good link for understanding this is http://paulsiu.wordpress.com/2007/06/23/does-on-demand-clock-modulation-odcm-conserve-battery/\n"
	print "   probable commands are\n"
	print "	clock set <number> : set the number to one of the below or in range between 0-100\n"
	print "		and i will automatically to the value nearest to\n"
	print "		12.5, 25.0, 37.5, 50.0, 63.5, 75, 87.5  (nehalem)\n"
	print "		sandy bridge supports 6.25% increments\n"
	print "		but, I (the tool) is not smart yet to distinguish between nehalem and sb\n"
	print "		so setting to 12.5 increment\n"
	print " 	     set <number> to 1 for 12.5%, 2 for 25%, 3 for 37.5%, 4 for 50%, \n"
	print"		5 for 63.5%, 6 for 75% and 7 for 87.5%\n"
	print	"	clock status	    : get clock modulation status\n"
	print	"	clock disable	    : disable clock modulation\n"
end

@duty_cycle_array = {0=>"reserved", 1=>'12.5% (default)', 2=>'25%', 3=>'37.5%', 4=>'50%', 5=>'63.5%',6=>'75%',7=>'87.5%'}

def get_clock_status()
	status = `rdmsr  0x19a --bitfield 4:4`
	print_command('rdmsr  0x19a --bitfield 4:4')
	if status.to_i == 0
		print "Clock modulation is disabled\n"
	else
		print "Clock modulation is enabled\n"
		status = `rdmsr  0x19a --bitfield 3:1`
		print_command('rdmsr  0x19a --bitfield 3:1')
		print "Duty Cycle is #{@duty_cycle_array[status.to_i]}\n"
	end
end

def set_clock_modulation(mult)
	if (mult.to_i<0) | (mult.to_i>8)
		print "Error: clock set <number>, where <number> should be between 1-7\n"
		print " 	     set <number> to 1 for 12.5%, 2 for 25%, 3 for 37.5%, 4 for 50%, \n"
		print"		5 for 63.5%, 6 for 75% and 7 for 87.5%\n"
		return
	end
	get_clock_status()

	mult = mult.to_i << 1
	mult = mult | 0x10
	
	for i in (0..12)
		status = "wrmsr  0x19a -p#{i} #{mult}"
		print_command(status)
		system(status)
	end
	get_clock_status()
end

def clock_disable()
	val = 0x0
	for i in (0..(num_cores()*2-1))
		status = "wrmsr  0x19a -p#{i} #{val}"
		print_command(status)
		system(status)
	end
	get_clock_status()
end	

def print_command( str )
	print "Running following command in sudo:  #{str} \n"
end

def print_turbo_status()
	status = `rdmsr  0x1a0 --bitfield 38:38`
	print_command('rdmsr  0x1a0 --bitfield 38:38')
	
	if status.to_i == 1
		print "Turbo is Disabled\n"
	else
		print "Turbo is Enabled\n"
	end
end

def enable_turbo_status()
	status = `rdmsr  0x1a0 --decimal`
	print "First checking status\n"
	print_command('rdmsr  0x1a0 --decimal')
	
	status = status.to_i & 0xBFFFFFFFFF #1 the 38th bit
	
	command = "wrmsr 0x1a0 #{status}"
	system(command)
	print_command(command)
	
	status = `rdmsr  0x1a0 --bitfield 38:38`
	print "Now checking if the update was successful or not\n"
	print_command('rdmsr  0x1a0 --bitfield 38:38')
	
	if status.to_i == 1
		print "Turbo is now Disabled, command failed and i dont know whyq\n"
	else
		print "Turbo is now Enabled, command succeeded\n"
	end
end

def disable_turbo_status()
	status = `rdmsr  0x1a0 --decimal`
	status = status.to_i | 0x4000000000 #1 the 38th bit
	print "First checking status\n"
	print_command('rdmsr  0x1a0 --decimal')
	
	command = "wrmsr 0x1a0 #{status}"
	system(command)
	print_command(command)
	
	status = `rdmsr  0x1a0 --bitfield 38:38`
	print "Now checking if the update was successful or not\n"
	print_command('rdmsr  0x1a0 --bitfield 38:38')

	if status.to_i == 1
		print "Turbo is now Disabled, command succeeded\n"
	else
		print "Turbo is now Enabled, command failed and i dont know why\n"
	end
end

def get_multiplier()
	if nehalem_or_sandybridge == 'Nehalem'
	    status = `rdmsr  0x198 --decimal`
	    print_command('rdmsr  0x198 --decimal')
	    status = status.to_i & 0xFFFF
	else
	    status = `rdmsr  0x198 --decimal --bitfield 15:8`
	    print_command('rdmsr  0x198 --decimal --bitfield 15:8')
	    status = status.to_i & 0xFF
	end
	print "  Current Multiplier is #{status}\n"
	
	status1 = `rdmsr  0xce --decimal --bitfield 47:40`
	print_command('rdmsr  0xce --decimal --bitfield 47:40')
	print "  Minimum Multiplier possible is #{status1.chomp}\n"
	
	status1 = `rdmsr  0xce --decimal --bitfield 15:8`
	print_command('rdmsr  0xce --decimal --bitfield 15:8')
	print "  Maximum Multiplier in Non-turbo mode is #{status1.chomp}\n"
	
	turbo1 = `rdmsr  0x1ad --decimal --bitfield 7:0`
	print_command('rdmsr  0x1ad --decimal --bitfield 7:0')
	turbo2 = `rdmsr  0x1ad --decimal --bitfield 15:8`
	print_command('rdmsr  0x1ad --decimal --bitfield 15:8')
	turbo3 = `rdmsr  0x1ad --decimal --bitfield 23:16`
	print_command('rdmsr  0x1ad --decimal --bitfield 23:16')
	turbo4 = `rdmsr  0x1ad --decimal --bitfield 31:24`
	print_command('rdmsr  0x1ad --decimal --bitfield 31:24')
	turbo5 = `rdmsr  0x1ad --decimal --bitfield 39:32`
	print_command('rdmsr  0x1ad --decimal --bitfield 39:32')
	turbo6 = `rdmsr  0x1ad --decimal --bitfield 47:40`
	print_command('rdmsr  0x1ad --decimal --bitfield 47:40')
	
	if num_cores()>=5
		print "  Maximum turbo limit with 1/2/3/4/5/6 cores active is #{turbo1.chomp}/#{turbo2.chomp}/#{turbo3.chomp}/#{turbo4.chomp}/#{turbo5.chomp}/#{turbo6.chomp}\n"
	elsif (num_cores()<=4 && num_cores()>2)
			print "  Maximum turbo limit with 1/2/3/4 cores active is #{turbo1.chomp}/#{turbo2.chomp}/#{turbo3.chomp}/#{turbo4.chomp}\n"
	else
			print "  Maximum turbo limit with 1/2 cores active is #{turbo1.chomp}/#{turbo2.chomp}\n"
	end
end

def set_multiplier(mult)
	status1 = `rdmsr  0xce --decimal --bitfield 47:40`
	minimum_multiplier = status1.chomp.to_i
	print_command('rdmsr  0xce --decimal --bitfield 47:40')
	
	turbo1 = `rdmsr  0x1ad --decimal --bitfield 7:0`
	maximum_multiplier = turbo1.chomp.to_i
	print_command('rdmsr  0x1ad --decimal --bitfield 7:0')
	
	if (mult < minimum_multiplier) | (mult > maximum_multiplier)
		print "You asked for a multiplier #{mult}\n"
		print "The range of input Multiplier is not in range\n"
		print "Please put it between #{minimum_multiplier} to #{maximum_multiplier}\n"

	else
		print "Don't worry if it prints that some wrmsr error message; i dont know the number of cores on your current machine so trying from 0..12"
		for i in (0..(num_cores()*2-1))
			status = "wrmsr  0x199 -p#{i} #{mult}"
			print_command(status)
			system(status)
		end
	end
	
	get_multiplier()
	#print "Current Multiplier is #{status}\n"
end

def get_power
	if nehalem_or_sandybridge != 'Nehalem'
	    print "POWER functions DON'T WORK ON SANDY BRIDGE (intel seems to have removed functionality)\n"
	else
	    status1 = `rdmsr  0x1ac --bitfield 14:0 --decimal`
	    print_command('rdmsr  0x1ac --bitfield 14:0 --decimal')
	
	    status2 = `rdmsr  0x1ac --bitfield 30:16 --decimal`
	    print_command('rdmsr  0x1ac --bitfield 30:16 --decimal')
	
	    print "Current TDP limit is #{status1.to_i * 1/8} watts, TDC limit is #{status2.to_i * 1/8} amps\n"
	end
end	

def set_tdp(limit)
	if nehalem_or_sandybridge != 'Nehalem'
	    print "POWER functions DON'T WORK ON SANDY BRIDGE (intel seems to have removed functionality)\n"
	else
	    status1 = `rdmsr  0x1ac --decimal`
	    print_command('rdmsr  0x1ac --decimal')
	
	    tdp = (status1.to_i & 0xFFFFFFFFFFFF8000) | (0x8000) | (limit.to_i/0.125)  #set bits 14:0, as resolution is 1/8 of a watt/amp 
	
	    status = "wrmsr  0x1ac #{tdp}"
	    print_command(status)
	    system(status)	
	end
end

def set_tdc(limit)
	if nehalem_or_sandybridge != 'Nehalem'
	    print "POWER functions DON'T WORK ON SANDY BRIDGE (intel seems to have removed functionality)\n"
	else
	    status1 = `rdmsr  0x1ac --decimal`
	    print_command('rdmsr  0x1ac --decimal')
	
	    tdc = (status1.to_i & 0xFFFFFFFF8000FFFF) | (0x80000000) | ((limit.to_i/0.125).to_i << 16)  #set bits 30:16, as resolution is 1/8 of a watt/amp 
	
	    status = "wrmsr  0x1ac #{tdc}"
	    print_command(status)
	    system(status)	
	end
end


#IGNORE THIS FUNCTION - NOT USING IT AT ALL
def disable_ida()
	#status = `rdmsr 0x199`
	#status = status.to_i(16)
	#p status
	
	#disable_turbo_string = 0x0100
	#system("wrmsr 0x199 #{disable_turbo_string}")

	#read the current opportunistic performance status and then write it. so first STATUS and then CTL
	curr_status = `rdmsr 0x198 --decimal`
	print  "current performance status 0x#{curr_status}\n"
	curr_status = (curr_status.to_i | 0x100).to_s(16)
	print  "in order to set ida=disable i will set bit 32 to get the following byte value 0x#{curr_status}\n"
	curr_command = "wrmsr 0x199 #{curr_status}"
	print "now executing #{curr_command}\n"
	system(curr_command)
end

print "\n\nThis script is totally experimental \n"
print "use it in superuser mode to get access to r/w access \n"
print "also i need msr-tools installed so that rdmsr and wrmsr can work in sudo\n"
print "write quit or ctrl+C to exit\n\n"
print "Now for the blurb on why you might find this script useful:\n"
print "Throttling cpu on battery is one place, some machines including W520\n"
print "have a wierd bios which switches off turbo when machine is booted in\n"
print "battery or some bios implement throttling even when the power is within limit\n"
print "and this tool should allow you to manually set the multiplier\n"
print "Whenever you run a command it will print out what goes in the background\n"
print "like what registers were read/written etc, this should allow one to even\n"
print "write different scripts to automatically run specifics multiplier in battery\n"
print "power and other modes.\n"
print "msr-tools are needed. Furthermore \"modprobe msr\" on command line needs to be executed\n"
print_command_list()

at_exit{print "exiting, thanks for using\n"}

while(1)
	print ">> "
	$input = STDIN.gets.chomp
	if $input.downcase.eql? "quit"
		break;
	end
	
	if $input.length == 0
		print_command_list()
	else 
		commands = $input.split
		case commands[0].downcase  # some commands have 0 level, eg just `help', others have two level 'turbo' and then status, enable and disable. multiplier get has 3 levels multiplier set 12 will set the multplier to 12
			when "help"
				print_command_list()
			when "turbo"
				if ! commands[1].nil?
					case commands[1].downcase
						when "status"
							print_turbo_status()
						when "enable"
							enable_turbo_status()
						when "disable"
							disable_turbo_status()
						else
							turbo_command_list()
					end
				else
					turbo_command_list()
				end
			when "multiplier"
				if !commands[1].nil?
					case commands[1].downcase
						when "get"
							get_multiplier()
						when "set"
							if !commands[2].nil?
								set_multiplier(commands[2].to_i)
							else
								multiplier_command_list()
							end
						else
							multiplier_command_list()
					end
				else
					multiplier_command_list()
				end			
			when "power"
				if !commands[1].nil?
					case commands[1].downcase
						when "get"
							get_power()
						when "set"
							if !commands[2].nil?  && !commands[3].nil?
								case commands[2].downcase
									when "tdp"
										set_tdp(commands[3])
									when "tdc"
										set_tdc(commands[3])
									else
										power_command_list()
								end
							else
								power_command_list()
							end
						else
							power_command_list()
					end
				else
					power_command_list()
				end	
			when "clock"
				if !commands[1].nil?
					case commands[1].downcase
						when "status"
							get_clock_status()
						when "set"
							if !commands[2].nil?
								set_clock_modulation(commands[2])
							else
								clock_command_list()
							end
						when "disable"
							clock_disable()
						else
							clock_command_list()
					end
				else
					clock_command_list()
				end					
            when "system"
                print "number of cores #{num_cores()}\n"
                print "trying to distinguish between nehalem/sandy bridge via AVX support... #{nehalem_or_sandybridge}\n"
            else
                print_command_list()
		end
	end
	
	
end

