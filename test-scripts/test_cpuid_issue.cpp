#include "stdio.h"
#include "memory.h"

static inline void cpuid (unsigned int info, unsigned int *eax, unsigned int *ebx,
                          unsigned int *ecx, unsigned int *edx)
{
    unsigned int _eax = info, _ebx, _ecx, _edx;
    asm volatile ("mov %%ebx, %%edi;" // save ebx (for PIC)
                  "cpuid;"
                  "mov %%ebx, %%esi;" // pass to caller
                  "mov %%edi, %%ebx;" // restore ebx
                  :"+a" (_eax), "=S" (_ebx), "=c" (_ecx), "=d" (_edx)
                  :      /* inputs: eax is handled above */
                  :"edi" /* clobbers: we hit edi directly */);
    if (eax) *eax = _eax;
    if (ebx) *ebx = _ebx;
    if (ecx) *ecx = _ecx;
    if (edx) *edx = _edx;
}

void get_familyinformation ()
{
      unsigned int a, b, c, d;
    // earlier i thought that NULL was the issue but
    cpuid (1, &b, NULL, NULL, NULL);
    //cpuid (1, &b, &a, &c, &d);
    //  printf ("eax %x\n", b);
	/*
    proc_info->stepping = b & 0x0000000F;	//bits 3:0
    proc_info->model = (b & 0x000000F0) >> 4;	//bits 7:4
    proc_info->family = (b & 0x00000F00) >> 8;	//bits 11:8
    proc_info->processor_type = (b & 0x00007000) >> 12;	//bits 13:12
    proc_info->extended_model = (b & 0x000F0000) >> 16;	//bits 19:16
    proc_info->extended_family = (b & 0x0FF00000) >> 20;	//bits 27:20
	*/
}
void  get_vendor (char *vendor_string)
{
    //get vendor name
    unsigned int a, b, c, d;
    cpuid (0, &a, &b, &c, &d);
    memcpy (vendor_string, &b, 4);
    memcpy (&vendor_string[4], &d, 4);
    memcpy (&vendor_string[8], &c, 4);
    vendor_string[12] = '\0';
    //        printf("Vendor %s\n",vendor_string);
}

int main() 
{
  printf("i will print success on quitting\n");
  fflush(stdout);

  char vendor_string[13];
  get_vendor(vendor_string);

      if ((strcmp (vendor_string, "GenuineIntel") == 0)) {
        printf ("i7z DEBUG: Found Intel Processor\n");
    } else {
        printf
        ("this was designed to be a intel proc utility. You can perhaps mod it for your machine?\n");
    }

  get_familyinformation();

  printf("working\n");
  fflush(stdout);
}
