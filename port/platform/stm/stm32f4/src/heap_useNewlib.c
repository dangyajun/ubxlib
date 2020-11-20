/**
 * \file heap_useNewlib.c
 * \brief Wrappers required to use newlib malloc-family within FreeRTOS.
 *
 * \par Overview
 * Route FreeRTOS memory management functions to newlib's malloc family.
 * Thus newlib and FreeRTOS share memory-management routines and memory pool,
 * and all newlib's internal memory-management requirements are supported.
 *
 * \author Dave Nadler
 * \date 7-August-2019
 * \version 23-Sep-2019 comments, check no malloc call inside ISR
 *
 * \see http://www.nadler.com/embedded/newlibAndFreeRTOS.html
 * \see https://sourceware.org/newlib/libc.html#Reentrancy
 * \see https://sourceware.org/newlib/libc.html#malloc
 * \see https://sourceware.org/newlib/libc.html#index-_005f_005fenv_005flock
 * \see https://sourceware.org/newlib/libc.html#index-_005f_005fmalloc_005flock
 * \see https://sourceforge.net/p/freertos/feature-requests/72/
 * \see http://www.billgatliff.com/newlib.html
 * \see http://wiki.osdev.org/Porting_Newlib
 * \see http://www.embecosm.com/appnotes/ean9/ean9-howto-newlib-1.0.html
 *
 *
 * \copyright
 * (c) Dave Nadler 2017-2019, All Rights Reserved.
 * Web:         http://www.nadler.com
 * email:       drn@nadler.com
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *
 * - Use or redistributions of source code must retain the above copyright notice,
 *   this list of conditions, ALL ORIGINAL COMMENTS, and the following disclaimer.
 *
 * - Redistributions in binary form must reproduce the above copyright notice, this
 *   list of conditions and the following disclaimer in the documentation and/or
 *   other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

// ================================================================================================
// =======================================  Configuration  ========================================
// These configuration symbols could be provided by from build...
#define STM_VERSION // use STM standard exported LD symbols
//#define SUPPORT_MALLOCS_INSIDE_ISRs // #define iff you have this crap code (ie unrepaired STM USB CDC)
#define SUPPORT_ISR_STACK_MONITOR     // #define to enable ISR (MSP) stack diagnostics
#define ISR_STACK_LENGTH_BYTES  512   // #define bytes to reserve for ISR (MSP) stack
// =======================================  Configuration  ========================================
// ================================================================================================


#include <stdlib.h> // maps to newlib...
#include <malloc.h> // mallinfo...
#include <errno.h>  // ENOMEM
#include <stdbool.h>
#include <stddef.h>

#include "newlib.h"
#if (__NEWLIB__ != 3) || (__NEWLIB_MINOR__ != 0)
#warning "This wrapper was verified for newlib version 3.0.0; please ensure newlib's external requirements for malloc-family are unchanged!"
#endif

#include "FreeRTOS.h" // defines public interface we're implementing here
#if !defined(configUSE_NEWLIB_REENTRANT) ||  (configUSE_NEWLIB_REENTRANT!=1)
#warning "#define configUSE_NEWLIB_REENTRANT 1 // Required for thread-safety of newlib sprintf, strtok, etc..."
// If you're *REALLY* sure you don't need FreeRTOS's newlib reentrancy support, remove this warning...
#endif
#include "task.h"

// ================================================================================================
// External routines required by newlib's malloc (sbrk/_sbrk, __malloc_lock/unlock)
// ================================================================================================

// Simplistic sbrk implementations assume stack grows downwards from top of memory,
// and heap grows upwards starting just after BSS.
// FreeRTOS normally allocates task stacks from a pool placed within BSS or DATA.
// Thus within a FreeRTOS task, stack pointer is always below end of BSS.
// When using this module, stacks are allocated from malloc pool, still always prior
// current unused heap area...
#if 0 // STM CubeMX 2018-2019 Incorrect Implementation (fails for FreeRTOS)
caddr_t _sbrk(int incr)
{
    extern char end asm("end"); // lowest unused RAM address, just beyond end of BSS.
    static char *heap_end;
    char *prev_heap_end;

    if (heap_end == 0) {
        heap_end = &end;
    }
    prev_heap_end = heap_end;
    if (heap_end + incr > stack_ptr) { // of course, always true for FreeRTOS task stacks
        errno = ENOMEM; // ...so first call inside a FreeRTOS task lands here
        return (caddr_t) -1;
    }
    heap_end += incr;
    return (caddr_t) prev_heap_end;
}
#endif

register char *stack_ptr asm("sp");

#ifdef STM_VERSION // Use STM CubeMX LD symbols for heap+stack area
// To avoid modifying STM LD file (and then having CubeMX trash it), use available STM symbols
// Unfortunately STM does not provide standardized markers for RAM suitable for heap!
// STM CubeMX-generated LD files provide the following symbols:
// end       /* aligned first word beyond BSS */
// _estack   /* one word beyond end of "RAM" Ram type memory, for STM32F429 0x20030000 */
// Kludge below uses CubeMX-generated symbols instead of sane LD definitions
#define __HeapBase  end
#define __HeapLimit _estack // except in K64F this was already adjusted in LD for stack...
static int heapBytesRemaining = 0;
// no DRN HEAP_SIZE symbol from LD... // that's (&__HeapLimit)-(&__HeapBase)
uint32_t TotalHeapSize; // publish for diagnostic routines; filled in first _sbrk call.
#else
// Note: DRN's K64F LD provided: __StackTop (byte beyond end of memory), __StackLimit, HEAP_SIZE, STACK_SIZE
// __HeapLimit was already adjusted to be below reserved stack area.
extern char HEAP_SIZE;  // make sure to define this symbol in linker LD command file
static int heapBytesRemaining = (int) &HEAP_SIZE; // that's (&__HeapLimit)-(&__HeapBase)
#endif

#ifdef MALLOCS_INSIDE_ISRs // STM code to avoid malloc within ISR (USB CDC stack)
// We can't use vTaskSuspendAll() within an ISR.
// STM's stunningly bad coding malpractice calls malloc within ISRs (for example, on USB connect function USBD_CDC_Init)
// So, we must just suspend/resume interrupts, lengthening max interrupt response time, aarrggg...
#define DRN_ENTER_CRITICAL_SECTION(_usis) { _usis = taskENTER_CRITICAL_FROM_ISR(); } // Disables interrupts (after saving prior state)
#define DRN_EXIT_CRITICAL_SECTION(_usis)  { taskEXIT_CRITICAL_FROM_ISR(_usis);     } // Re-enables interrupts (unless already disabled prior taskENTER_CRITICAL)
#else
#define DRN_ENTER_CRITICAL_SECTION(_usis) vTaskSuspendAll(); // Note: safe to use before FreeRTOS scheduler started, but not in ISR
#define DRN_EXIT_CRITICAL_SECTION(_usis)  xTaskResumeAll();  // Note: safe to use before FreeRTOS scheduler started, but not in ISR
#endif

#ifndef NDEBUG
static int totalBytesProvidedBySBRK = 0;
#endif
extern char __HeapBase, __HeapLimit;  // make sure to define these symbols in linker LD command file

// Return the value of "heap bytes remaining", which
// is the size not yet passed to newlib by malloc().
// Since newlib only asks for memory when it needs
// more and it never comes back this is a measure of
// the minimum heap remaining EVER.
int uPortInternalGetSbrkFreeBytes()
{
    return heapBytesRemaining;
}

//! _sbrk_r version supporting reentrant newlib (depends upon above symbols defined by linker control file).
void *_sbrk_r(struct _reent *pReent, int incr)
{
#ifdef MALLOCS_INSIDE_ISRs // block interrupts during free-storage use
    UBaseType_t usis; // saved interrupt status
#endif
    static char *currentHeapEnd = &__HeapBase;
#ifdef STM_VERSION // Use STM CubeMX LD symbols for heap
    if (TotalHeapSize == 0) {
        TotalHeapSize = heapBytesRemaining = (int)((&__HeapLimit) - (&__HeapBase)) - ISR_STACK_LENGTH_BYTES;
    };
#endif
    char *limit = (xTaskGetSchedulerState() == taskSCHEDULER_NOT_STARTED) ?
                  stack_ptr   :  // Before scheduler is started, limit is stack pointer (risky!)
                  &__HeapLimit -
                  ISR_STACK_LENGTH_BYTES; // Once running, OK to reuse all remaining RAM except ISR stack (MSP) stack
    DRN_ENTER_CRITICAL_SECTION(usis);
    char *previousHeapEnd = currentHeapEnd;
    if (currentHeapEnd + incr > limit) {
        // Ooops, no more memory available...
#if( configUSE_MALLOC_FAILED_HOOK == 1 )
        {
            extern void vApplicationMallocFailedHook( void );
            DRN_EXIT_CRITICAL_SECTION(usis);
            vApplicationMallocFailedHook();
        }
#elif defined(configHARD_STOP_ON_MALLOC_FAILURE)
        // If you want to alert debugger or halt...
        // WARNING: brkpt instruction may prevent watchdog operation...
        while (1) {
            __asm("bkpt #0");
        }; // Stop in GUI as if at a breakpoint (if debugging, otherwise loop forever)
#else
        // Default, if you prefer to believe your application will gracefully trap out-of-memory...
        pReent->_errno = ENOMEM; // newlib's thread-specific errno
        DRN_EXIT_CRITICAL_SECTION(usis);
#endif
        return (char *) -1; // the malloc-family routine that called sbrk will return 0
    }
    // 'incr' of memory is available: update accounting and return it.
    currentHeapEnd += incr;
    heapBytesRemaining -= incr;
#ifndef NDEBUG
    totalBytesProvidedBySBRK += incr;
#endif
    DRN_EXIT_CRITICAL_SECTION(usis);
    return (char *) previousHeapEnd;
}
//! non-reentrant sbrk uses is actually reentrant by using current context
// ... because the current _reent structure is pointed to by global _impure_ptr
char *sbrk(int incr)
{
    return _sbrk_r(_impure_ptr, incr);
}
//! _sbrk is a synonym for sbrk.
char *_sbrk(int incr)
{
    return sbrk(incr);
};

#ifdef MALLOCS_INSIDE_ISRs // block interrupts during free-storage use
static UBaseType_t malLock_uxSavedInterruptStatus;
#endif
void __malloc_lock(struct _reent *r)
{
#if defined(MALLOCS_INSIDE_ISRs)
    DRN_ENTER_CRITICAL_SECTION(malLock_uxSavedInterruptStatus);
#else
    bool insideAnISR = xPortIsInsideInterrupt();
    configASSERT( !insideAnISR ); // Make damn sure no more mallocs inside ISRs!!
    vTaskSuspendAll();
#endif
};
void __malloc_unlock(struct _reent *r)
{
#if defined(MALLOCS_INSIDE_ISRs)
    DRN_EXIT_CRITICAL_SECTION(malLock_uxSavedInterruptStatus);
#else
    (void)xTaskResumeAll();
#endif
};

// newlib also requires implementing locks for the application's environment memory space,
// accessed by newlib's setenv() and getenv() functions.
// As these are trivial functions, momentarily suspend task switching (rather than semaphore).
// ToDo: Move __env_lock/unlock to a separate newlib helper file.
void __env_lock()
{
    vTaskSuspendAll();
};
void __env_unlock()
{
    (void)xTaskResumeAll();
};

// ================================================================================================
// Implement FreeRTOS's memory API using newlib-provided malloc family.
// ================================================================================================

void *pvPortMalloc( size_t xSize ) PRIVILEGED_FUNCTION {
    void *p = malloc(xSize);
    return p;
}
void vPortFree( void *pv ) PRIVILEGED_FUNCTION {
    free(pv);
};

size_t xPortGetFreeHeapSize( void ) PRIVILEGED_FUNCTION {
    struct mallinfo mi = mallinfo(); // available space now managed by newlib
    return mi.fordblks + heapBytesRemaining; // plus space not yet handed to newlib by sbrk
}

// GetMinimumEverFree is not available in newlib's malloc implementation.
// So, no implementation is provided: size_t xPortGetMinimumEverFreeHeapSize( void ) PRIVILEGED_FUNCTION;

//! No implementation needed, but stub provided in case application already calls vPortInitialiseBlocks
void vPortInitialiseBlocks( void ) PRIVILEGED_FUNCTION {};
