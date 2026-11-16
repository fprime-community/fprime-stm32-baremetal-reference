#include "FprimeStm32BaremetalReference/Components/RadioTest/RadioTest.hpp"
#include "FprimeStm32BaremetalReference/Components/RadioTest/FppConstantsAc.hpp"

#include <Fw/Types/Assert.hpp>

namespace ReferenceDeployment {

RadioTest::RadioTest(const char* name) : RadioTestComponentBase(name), m_frequencyId(0) {}

void RadioTest::configure(FwPrmIdType frequencyId) {
    this->m_frequencyId = frequencyId;
}

Fw::ParamValid RadioTest::getParameter_handler(FwIndexType portNum, FwPrmIdType id, Fw::ParamBuffer& value) {
    (void)portNum;
    const Fw::ParamValid validity = this->getStoredParameter_out(0, id, value);
    if (validity != Fw::ParamValid::INVALID || id != this->m_frequencyId) {
        return validity;
    }
    value.resetSer();
    const Fw::SerializeStatus status = value.serializeFrom(static_cast<U32>(RADIO_DEFAULT_FREQUENCY_HZ));
    FW_ASSERT(status == Fw::FW_SERIALIZE_OK, static_cast<FwAssertArgType>(status));
    return Fw::ParamValid::VALID;
}

void RadioTest::receive_handler(FwIndexType portNum, Fw::Buffer& data, const ComCfg::FrameContext& context) {
    (void)portNum;
    constexpr FwSizeType SEGMENT_SIZE = 32;
    const U8* bytes = data.getData();
    FW_ASSERT(bytes != nullptr || data.getSize() == 0);
    for (FwSizeType offset = 0; offset < data.getSize(); offset += SEGMENT_SIZE) {
        char text[SEGMENT_SIZE + 1] = {};
        FwSizeType length = data.getSize() - offset;
        if (length > SEGMENT_SIZE) {
            length = SEGMENT_SIZE;
        }
        for (FwSizeType i = 0; i < length; ++i) {
            const U8 c = bytes[offset + i];
            text[i] = (c >= 32 && c <= 126) ? static_cast<char>(c) : '.';
        }
        this->log_ACTIVITY_HI_ReplySegment(static_cast<U32>(offset), static_cast<U32>(data.getSize()),
                                           Fw::LogStringArg(text));
    }
    this->returnBuffer_out(0, data, context);
}

}  // namespace ReferenceDeployment
