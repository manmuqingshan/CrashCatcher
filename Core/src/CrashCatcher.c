/* Copyright (C) 2014  Adam Green (https://github.com/adamgreen)

   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at

       http://www.apache.org/licenses/LICENSE-2.0

   Unless required by applicable law or agreed to in writing, software
   distributed under the License is distributed on an "AS IS" BASIS,
   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
   See the License for the specific language governing permissions and
   limitations under the License.
*/
#include <CrashCatcher.h>
#include "CrashCatcherPriv.h"


/* Test harness will define this value on 64-bit machine to provide upper 32-bits of pointer addresses. */
extern uint64_t g_crashCatcherTestBaseAddress;


/* Fault handler will switch MSP to use this area as the stack while CrashCatcher code is running. */
uint32_t g_crashCatcherStack[256];


typedef struct
{
    const CrashCatcherExceptionRegisters* pExceptionRegisters;
    const CrashCatcherStackedRegisters*   pSP;
    uint32_t                              sp;
} Object;


static Object initStackPointers(const CrashCatcherExceptionRegisters* pExceptionRegisters);
static uint32_t getAddressOfExceptionStack(const CrashCatcherExceptionRegisters* pExceptionRegisters);
static const void* uint32AddressToPointer(uint32_t address);
static void advanceStackPointerToValueBeforeException(Object* pObject);
static void dumpSignature(const Object* pObject);
static void dumpR0toR3(const Object* pObject);
static void dumpR4toR11(const Object* pObject);
static void dumpR12(const Object* pObject);
static void dumpSP(const Object* pObject);
static void dumpLR_PC_PSR(const Object* pObject);
static void dumpExceptionPSR(const Object* pObject);
static void dumpMemoryRegions(void);


void CrashCatcher_Entry(const CrashCatcherExceptionRegisters* pExceptionRegisters)
{
    Object object = initStackPointers(pExceptionRegisters);
    advanceStackPointerToValueBeforeException(&object);

    do
    {
        CrashCatcher_DumpStart();
        dumpSignature(&object);
        dumpR0toR3(&object);
        dumpR4toR11(&object);
        dumpR12(&object);
        dumpSP(&object);
        dumpLR_PC_PSR(&object);
        dumpExceptionPSR(&object);
        dumpMemoryRegions();
    }
    while (CrashCatcher_DumpEnd() == CRASH_CATCHER_TRY_AGAIN);
}

static Object initStackPointers(const CrashCatcherExceptionRegisters* pExceptionRegisters)
{
    Object object;
    object.pExceptionRegisters = pExceptionRegisters;
    object.sp = getAddressOfExceptionStack(pExceptionRegisters);
    object.pSP = uint32AddressToPointer(object.sp);
    return object;
}

static uint32_t getAddressOfExceptionStack(const CrashCatcherExceptionRegisters* pExceptionRegisters)
{
    if (pExceptionRegisters->exceptionLR & LR_PSP)
        return pExceptionRegisters->psp;
    else
        return pExceptionRegisters->msp;
}

static const void* uint32AddressToPointer(uint32_t address)
{
    if (sizeof(uint32_t*) == 8)
        return (const void*)(unsigned long)((uint64_t)address | g_crashCatcherTestBaseAddress);
    else
        return (const void*)(unsigned long)address;
}

static void advanceStackPointerToValueBeforeException(Object* pObject)
{
    /* Cortex-M processor pushed 8 registers on the stack. */
    pObject->sp += 8 * sizeof(uint32_t);
    /* Cortex-M processor may also have had to force 8-byte alignment before auto stacking registers. */
    pObject->sp |= (pObject->pSP->psr & PSR_STACK_ALIGN) ? 4 : 0;
}

static void dumpSignature(const Object* pObject)
{
    static const uint8_t signature[4] = {CRASH_CATCHER_SIGNATURE_BYTE0,
                                         CRASH_CATCHER_SIGNATURE_BYTE1,
                                         CRASH_CATCHER_VERSION_MAJOR,
                                         CRASH_CATCHER_VERSION_MINOR};

    CrashCatcher_DumpMemory(signature, CRASH_CATCHER_BYTE, sizeof(signature));
}

static void dumpR0toR3(const Object* pObject)
{
    CrashCatcher_DumpMemory(&pObject->pSP->r0, CRASH_CATCHER_BYTE, 4 * sizeof(uint32_t));
}

static void dumpR4toR11(const Object* pObject)
{
    CrashCatcher_DumpMemory(&pObject->pExceptionRegisters->r4, CRASH_CATCHER_BYTE, (11 - 4 + 1) * sizeof(uint32_t));
}

static void dumpR12(const Object* pObject)
{
    CrashCatcher_DumpMemory(&pObject->pSP->r12, CRASH_CATCHER_BYTE, sizeof(uint32_t));
}

static void dumpSP(const Object* pObject)
{
    CrashCatcher_DumpMemory(&pObject->sp, CRASH_CATCHER_BYTE, sizeof(uint32_t));
}

static void dumpLR_PC_PSR(const Object* pObject)
{
    CrashCatcher_DumpMemory(&pObject->pSP->lr, CRASH_CATCHER_BYTE, 3 * sizeof(uint32_t));
}

static void dumpExceptionPSR(const Object* pObject)
{
    CrashCatcher_DumpMemory(&pObject->pExceptionRegisters->exceptionPSR, CRASH_CATCHER_BYTE, sizeof(uint32_t));
}

static void dumpMemoryRegions(void)
{
    const CrashCatcherMemoryRegion* pRegion = CrashCatcher_GetMemoryRegions();
    while (pRegion && pRegion->startAddress != 0xFFFFFFFF)
    {
        /* Just dump the two addresses in pRegion.  The element size isn't required. */
        CrashCatcher_DumpMemory(pRegion, CRASH_CATCHER_BYTE, 2 * sizeof(uint32_t));
        CrashCatcher_DumpMemory(uint32AddressToPointer(pRegion->startAddress),
                                pRegion->elementSize,
                                (pRegion->endAddress - pRegion->startAddress) / pRegion->elementSize);
        pRegion++;
    }
}
