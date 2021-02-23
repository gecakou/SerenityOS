/*
 * Copyright (c) 2020, Liav A. <liavalb@hotmail.co.il>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <Kernel/ACPI/DynamicParser.h>
#include <Kernel/ACPI/Parser.h>

namespace Kernel {
namespace ACPI {

UNMAP_AFTER_INIT DynamicParser::DynamicParser(PhysicalAddress rsdp)
    : IRQHandler(9)
    , Parser(rsdp)
{
    klog() << "ACPI: Dynamic Parsing Enabled, Can parse AML";
}

void DynamicParser::handle_irq(const RegisterState&)
{
    // FIXME: Implement IRQ handling of ACPI signals!
    VERIFY_NOT_REACHED();
}

void DynamicParser::enable_aml_interpretation()
{
    // FIXME: Implement AML Interpretation
    VERIFY_NOT_REACHED();
}
void DynamicParser::enable_aml_interpretation(File&)
{
    // FIXME: Implement AML Interpretation
    VERIFY_NOT_REACHED();
}
void DynamicParser::enable_aml_interpretation(u8*, u32)
{
    // FIXME: Implement AML Interpretation
    VERIFY_NOT_REACHED();
}
void DynamicParser::disable_aml_interpretation()
{
    // FIXME: Implement AML Interpretation
    VERIFY_NOT_REACHED();
}
void DynamicParser::try_acpi_shutdown()
{
    // FIXME: Implement AML Interpretation to perform ACPI shutdown
    VERIFY_NOT_REACHED();
}

void DynamicParser::build_namespace()
{
    // FIXME: Implement AML Interpretation to build the ACPI namespace
    VERIFY_NOT_REACHED();
}

}
}
