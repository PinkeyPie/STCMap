#include "DxRenderTarget.h"

DxRenderTarget::DxRenderTarget(std::initializer_list<Ptr<DxTexture> > colorAttachments, Ptr<DxTexture> depthAttachment) {
    uint32 width = 0;
    uint32 height = 0;

    NumAttachments = 0;
    for (const Ptr<DxTexture>& t : colorAttachments) {
        if (t) {
            width = t->Width;
            height = t->Height;
            RTV[NumAttachments++] = t->RtvHandles;
        }
    }
    DSV = depthAttachment ? depthAttachment->DsvHandle : DxDsvDescriptorHandle{};

    assert(NumAttachments > 0 or depthAttachment != nullptr);

    width = (NumAttachments > 0) ? width : depthAttachment->Width;
    height = (NumAttachments > 0) ? height : depthAttachment->Height;

    Viewport = { 0.f, 0.f, (float)width, (float)height, 0.f, 1.f };
}
