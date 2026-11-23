#include "FprimeStm32BaremetalReference/Components/RadioTest/RadioTest.hpp"
#include "FprimeStm32BaremetalReference/Components/RadioTest/FppConstantsAc.hpp"

#include <Fw/Types/Assert.hpp>
#include <cstring>

namespace ReferenceDeployment {

RadioTest::RadioTest(const char* name)
    : RadioTestComponentBase(name), m_frequencyId(0), m_lock(), m_payload{}, m_pending(false), m_submitting(false),
      m_rejected(false) {}

void RadioTest::configure(FwPrmIdType frequencyId) {
    this->m_frequencyId = frequencyId;
}

void RadioTest::SEND_STRING_cmdHandler(FwOpcodeType opCode, U32 cmdSeq, const Fw::CmdStringArg& payload) {
    const FwSizeType size = payload.length();
    bool accepted = false;
    {
        Os::ScopeLock lock(this->m_lock);
        if (size > 0 && size <= sizeof(this->m_payload) && !this->m_pending) {
            std::memcpy(this->m_payload, payload.toChar(), size);
            this->m_pending = true;
            this->m_submitting = true;
            this->m_rejected = false;
            accepted = true;
        }
    }
    if (accepted) {
        Fw::Buffer buffer(this->m_payload, size);
        ComCfg::FrameContext context;
        this->transmit_out(0, buffer, context);
        Os::ScopeLock lock(this->m_lock);
        this->m_submitting = false;
        accepted = !this->m_rejected;
        if (!accepted) {
            this->m_pending = false;
        }
    }
    if (accepted) {
        this->log_ACTIVITY_HI_TestPayloadQueued(size);
        this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::OK);
    } else {
        this->log_WARNING_HI_TestSendRejected();
        this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::EXECUTION_ERROR);
    }
}

void RadioTest::sendReturn_handler(FwIndexType portNum, Fw::Buffer& data, const ComCfg::FrameContext& context) {
    (void)portNum;
    (void)context;
    Os::ScopeLock lock(this->m_lock);
    FW_ASSERT(this->m_pending && data.getData() == this->m_payload);
    if (this->m_submitting) {
        this->m_rejected = true;
    } else {
        this->m_pending = false;
    }
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
