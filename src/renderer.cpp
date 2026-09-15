#include "renderer.hpp"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <numbers>
#include <stdexcept>

namespace beer {
namespace {
void hr(HRESULT result) {
    if (FAILED(result))
        throw std::runtime_error("Graphics device failure");
}
D2D1_COLOR_F rgba(uint32_t rgb, float a = 1) {
    return D2D1::ColorF(rgb, a);
}
ComPtr<IDWriteFontCollection1> widget_fonts(IDWriteFactory *factory) {
    ComPtr<IDWriteFactory3> modern;
    ComPtr<IDWriteFontSetBuilder> builder;
    if (FAILED(factory->QueryInterface(IID_PPV_ARGS(&modern))) ||
        FAILED(modern->CreateFontSetBuilder(&builder)))
        return {};
    wchar_t directory[MAX_PATH]{};
    const auto length = GetWindowsDirectoryW(directory, MAX_PATH);
    if (!length || length >= MAX_PATH)
        return {};
    // Use installed Windows fonts, without enumerating the entire system collection.
    for (const auto *name : {L"segoeui.ttf", L"seguisb.ttf"}) {
        const auto path = std::wstring(directory) + L"\\Fonts\\" + name;
        ComPtr<IDWriteFontFaceReference> face;
        if (FAILED(modern->CreateFontFaceReference(path.c_str(), nullptr, 0, DWRITE_FONT_SIMULATIONS_NONE,
                                                   &face)) ||
            FAILED(builder->AddFontFaceReference(face.Get())))
            return {};
    }
    ComPtr<IDWriteFontSet> set;
    ComPtr<IDWriteFontCollection1> collection;
    if (FAILED(builder->CreateFontSet(&set)) ||
        FAILED(modern->CreateFontCollectionFromFontSet(set.Get(), &collection)))
        return {};
    return collection;
}
} // namespace
Renderer::Renderer(HWND window, bool low_memory) : window_(window), low_memory_(low_memory) {
    hr(D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, IID_PPV_ARGS(&factory_)));
    if (low_memory_) {
        software_ = true;
        auto properties = D2D1::RenderTargetProperties(
            D2D1_RENDER_TARGET_TYPE_SOFTWARE,
            D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED), 96, 96);
        hr(factory_->CreateDCRenderTarget(&properties, &cpu_target_));
        hr(cpu_target_.As(&dc_));
    } else {
        UINT flags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
        HRESULT result = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, flags, nullptr, 0,
                                           D3D11_SDK_VERSION, &d3d_, nullptr, &immediate_);
        if (FAILED(result)) {
            software_ = true;
            hr(D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, flags, nullptr, 0, D3D11_SDK_VERSION,
                                 &d3d_, nullptr, &immediate_));
        }
        ComPtr<IDXGIDevice> dxgi;
        hr(d3d_.As(&dxgi));
        hr(factory_->CreateDevice(dxgi.Get(), &device_));
        hr(device_->CreateDeviceContext(D2D1_DEVICE_CONTEXT_OPTIONS_NONE, &gpu_context_));
        gpu_context_->SetUnitMode(D2D1_UNIT_MODE_PIXELS);
        hr(gpu_context_.As(&dc_));
        ComPtr<IDXGIAdapter> adapter;
        hr(dxgi->GetAdapter(&adapter));
        ComPtr<IDXGIFactory2> dxgi_factory;
        hr(adapter->GetParent(IID_PPV_ARGS(&dxgi_factory)));
        DXGI_SWAP_CHAIN_DESC1 description{};
        description.Width = 1;
        description.Height = 1;
        description.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
        description.SampleDesc.Count = 1;
        description.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        description.BufferCount = 2;
        description.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
        description.AlphaMode = DXGI_ALPHA_MODE_PREMULTIPLIED;
        description.Scaling = DXGI_SCALING_STRETCH;
        hr(dxgi_factory->CreateSwapChainForComposition(d3d_.Get(), &description, nullptr, &swap_));
        hr(DCompositionCreateDevice(dxgi.Get(), IID_PPV_ARGS(&composition_)));
        hr(composition_->CreateTargetForHwnd(window_, TRUE, &target_));
        hr(composition_->CreateVisual(&root_));
        hr(root_->SetContent(swap_.Get()));
        hr(target_->SetRoot(root_.Get()));
        hr(composition_->Commit());
    }
    dc_->SetTextAntialiasMode(D2D1_TEXT_ANTIALIAS_MODE_GRAYSCALE);
    hr(DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory),
                           reinterpret_cast<IUnknown **>(write_.GetAddressOf())));
    fonts_ = widget_fonts(write_.Get());
    hr(dc_->CreateSolidColorBrush(rgba(0xffffff), &brush_));
    hr(dc_->CreateLayer(&clip_layer_));
    hr(factory_->CreateRoundedRectangleGeometry(D2D1::RoundedRect(D2D1::RectF(47, 46, 167, 220), 13, 13),
                                                &inside_));
    RECT rc{};
    GetClientRect(window_, &rc);
    resize((std::max)(1L, rc.right), (std::max)(1L, rc.bottom));
}
Renderer::~Renderer() {
    dc_.Reset();
    cpu_target_.Reset();
    release_readback();
}
void Renderer::release_readback() {
    staging_.Reset();
    if (memory_dc_) {
        SelectObject(memory_dc_, old_dib_);
        DeleteObject(dib_);
        DeleteDC(memory_dc_);
    }
    memory_dc_ = nullptr;
    dib_ = nullptr;
    pixels_ = nullptr;
}
void Renderer::resize(UINT width, UINT height) {
    if ((bitmap_ || memory_dc_) && width == width_ && height == height_)
        return;
    width_ = (std::max)(1u, width);
    height_ = (std::max)(1u, height);
    release_readback();
    if (low_memory_) {
        allocate_dib();
        RECT bounds{0, 0, static_cast<LONG>(width_), static_cast<LONG>(height_)};
        hr(cpu_target_->BindDC(memory_dc_, &bounds));
        return;
    }
    gpu_context_->SetTarget(nullptr);
    bitmap_.Reset();
    hr(swap_->ResizeBuffers(0, width_, height_, DXGI_FORMAT_UNKNOWN, 0));
    ComPtr<IDXGISurface> surface;
    hr(swap_->GetBuffer(0, IID_PPV_ARGS(&surface)));
    auto properties =
        D2D1::BitmapProperties1(D2D1_BITMAP_OPTIONS_TARGET | D2D1_BITMAP_OPTIONS_CANNOT_DRAW,
                                D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED));
    hr(gpu_context_->CreateBitmapFromDxgiSurface(surface.Get(), &properties, &bitmap_));
    gpu_context_->SetTarget(bitmap_.Get());
}
void Renderer::color(uint32_t rgb, float alpha) {
    brush_->SetColor(rgba(rgb, alpha));
}
void Renderer::round(float x, float y, float w, float h, float r, uint32_t rgb, float alpha) {
    color(rgb, alpha);
    dc_->FillRoundedRectangle(D2D1::RoundedRect(D2D1::RectF(x, y, x + w, y + h), r, r), brush_.Get());
}
void Renderer::update_theme(const Theme &t) {
    if (!cached_valid_ || t.fill_left != cached_.fill_left || t.fill_top != cached_.fill_top ||
        t.fill_right != cached_.fill_right || t.fill_bottom != cached_.fill_bottom) {
        inside_.Reset();
        hr(factory_->CreateRoundedRectangleGeometry(
            D2D1::RoundedRect(D2D1::RectF(t.fill_left, t.fill_top, t.fill_right, t.fill_bottom), 13, 13),
            &inside_));
    }
    if (!cached_valid_ || t.text_size != cached_.text_size) {
        text_.Reset();
        label_layout_.Reset();
        caption_layout_.Reset();
        hr(write_->CreateTextFormat(L"Segoe UI", fonts_.Get(), DWRITE_FONT_WEIGHT_SEMI_BOLD,
                                    DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, t.text_size,
                                    L"ru-RU", &text_));
        text_->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
        text_->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
        small_text_.Reset();
        hr(write_->CreateTextFormat(L"Segoe UI", fonts_.Get(), DWRITE_FONT_WEIGHT_NORMAL,
                                    DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 11, L"ru-RU",
                                    &small_text_));
        small_text_->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
        small_text_->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
    }
    if (!cached_valid_ || t.liquid_color != cached_.liquid_color || t.liquid_alpha != cached_.liquid_alpha ||
        t.glass_alpha != cached_.glass_alpha) {
        const auto c = rgba(t.liquid_color, t.liquid_alpha);
        D2D1_GRADIENT_STOP stops[] = {
            {0, {c.r * .7f, c.g * .6f, c.b * .6f, c.a}},
            {.35f, {(std::min)(1.f, c.r * 1.15f), (std::min)(1.f, c.g * 1.15f), c.b * 1.1f, c.a}},
            {.7f, c},
            {1, {c.r * .75f, c.g * .65f, c.b * .65f, c.a}}};
        ComPtr<ID2D1GradientStopCollection> collection;
        hr(dc_->CreateGradientStopCollection(stops, 4, &collection));
        beer_.Reset();
        hr(dc_->CreateLinearGradientBrush(
            D2D1::LinearGradientBrushProperties(D2D1::Point2F(47, 0), D2D1::Point2F(167, 0)),
            collection.Get(), &beer_));
        D2D1_GRADIENT_STOP glass_stops[] = {{0, rgba(0xe4f6ff, t.glass_alpha)},
                                            {.25f, rgba(0xa4cde1, t.glass_alpha * .15f)},
                                            {.8f, rgba(0xbde7fa, t.glass_alpha * .3f)},
                                            {1, rgba(0xf2fbff, t.glass_alpha)}};
        collection.Reset();
        hr(dc_->CreateGradientStopCollection(glass_stops, 4, &collection));
        glass_.Reset();
        hr(dc_->CreateLinearGradientBrush(
            D2D1::LinearGradientBrushProperties(D2D1::Point2F(38, 0), D2D1::Point2F(177, 0)),
            collection.Get(), &glass_));
    }
    cached_ = t;
    cached_valid_ = true;
}
void Renderer::draw(const Theme &t, double remaining, double secondary, const std::wstring &label,
                    const std::wstring &caption, double phase, HWND layered) {
    update_theme(t);
    if (!label_layout_ || cached_label_ != label) {
        label_layout_.Reset();
        hr(write_->CreateTextLayout(label.c_str(), static_cast<UINT32>(label.size()), text_.Get(), 180, 36,
                                    &label_layout_));
        cached_label_ = label;
    }
    if (!caption_layout_ || cached_caption_ != caption) {
        caption_layout_.Reset();
        hr(write_->CreateTextLayout(caption.c_str(), static_cast<UINT32>(caption.size()), small_text_.Get(),
                                    184, 15, &caption_layout_));
        cached_caption_ = caption;
    }
    dc_->BeginDraw();
    dc_->SetTransform(D2D1::Matrix3x2F::Identity());
    dc_->Clear(D2D1::ColorF(0, 0.f));
    dc_->SetTransform(D2D1::Matrix3x2F::Scale(width_ / 240.f, height_ / 300.f));
    const float level = static_cast<float>(std::clamp(remaining, 0., 100.));
    if (!t.ring) {
        color(0xc8e5f1, t.glass_alpha);
        dc_->DrawRoundedRectangle(D2D1::RoundedRect(D2D1::RectF(166, 84, 207, 177), 19, 19), brush_.Get(),
                                  12);
        dc_->FillRoundedRectangle(D2D1::RoundedRect(D2D1::RectF(38, 34, 177, 233), 21, 21), glass_.Get());
        color(0xb5cbd7, .75f);
        dc_->DrawRoundedRectangle(D2D1::RoundedRect(D2D1::RectF(38, 34, 177, 233), 21, 21), brush_.Get(), 2);
        dc_->PushLayer(D2D1::LayerParameters(D2D1::InfiniteRect(), inside_.Get()), clip_layer_.Get());
        if (remaining >= 0 && level > 0) {
            const float top = t.fill_bottom - (t.fill_bottom - t.fill_top) * level / 100;
            const float center = (t.fill_left + t.fill_right) / 2, width = t.fill_right - t.fill_left;
            dc_->FillRectangle(D2D1::RectF(t.fill_left, top, t.fill_right, t.fill_bottom), beer_.Get());
            const float wave = t.decoration ? static_cast<float>(std::sin(phase * 1.7) * t.waves * 2) : 0.f;
            color(0xffe6a2, t.liquid_alpha);
            dc_->FillEllipse(D2D1::Ellipse(D2D1::Point2F(center, top + 3), width / 2 + 2, 3 + wave),
                             brush_.Get());
            dc_->PushAxisAlignedClip(D2D1::RectF(t.fill_left, top, t.fill_right, t.fill_bottom),
                                     D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
            for (int i = 0; i < t.bubbles && i < 24; i++) {
                const float x = t.fill_left + 4 + static_cast<float>((i * 37) % 103) / 103 * (width - 8);
                const float y =
                    t.fill_bottom - static_cast<float>(std::fmod(i * 13.7 + (t.decoration ? phase * 13 : 0),
                                                                 (std::max)(1.f, t.fill_bottom - top)));
                color(0xfff0bf, .48f);
                dc_->DrawEllipse(
                    D2D1::Ellipse(D2D1::Point2F(x, y), 1.2f + (i % 3) * .4f, 1.6f + (i % 3) * .4f),
                    brush_.Get(), .8f);
            }
            dc_->PopAxisAlignedClip();
            if (t.foam > 0) {
                color(0xfff3d0, t.foam);
                for (int i = 0; i < 10; i++)
                    dc_->FillEllipse(D2D1::Ellipse(D2D1::Point2F(t.fill_left + i * width / 9,
                                                                 top + 3 + ((i % 3) - 1) * wave),
                                                   width / 12, 2 + t.foam * 5),
                                     brush_.Get());
            }
        }
        dc_->PopLayer();
        round(48, 60, 4, 151, 2, 0xffffff, t.glass_alpha);
        round(77, 73, 3, 135, 1.5f, 0xffffff, t.glass_alpha * .4f);
        round(145, 72, 3, 136, 1.5f, 0xffffff, t.glass_alpha * .35f);
        round(59, 224, 95, 4, 2, 0xedfbff, t.glass_alpha);
        color(0xeefaff, t.glass_alpha);
        dc_->DrawEllipse(D2D1::Ellipse(D2D1::Point2F(107, 40), 64, 7), brush_.Get(), 3);
    } else {
        color(0x8594a2, .35f);
        dc_->DrawEllipse(D2D1::Ellipse(D2D1::Point2F(120, 130), 77, 77), brush_.Get(), 14);
        if (remaining >= 0) {
            color(t.liquid_color, t.liquid_alpha);
            const int steps = static_cast<int>(level * 2);
            for (int i = 0; i < steps; i++) {
                const double a = (-90 + i * 1.8) * std::numbers::pi / 180,
                             b = (-90 + (i + 1) * 1.8) * std::numbers::pi / 180;
                dc_->DrawLine(D2D1::Point2F(120 + 77 * static_cast<float>(cos(a)),
                                            130 + 77 * static_cast<float>(sin(a))),
                              D2D1::Point2F(120 + 77 * static_cast<float>(cos(b)),
                                            130 + 77 * static_cast<float>(sin(b))),
                              brush_.Get(), 14);
            }
        }
    }
    round(27, 241, 186, 34, 17, 0x152330, .96f);
    color(t.text_color);
    dc_->DrawTextLayout(D2D1::Point2F(30, 240), label_layout_.Get(), brush_.Get(),
                        D2D1_DRAW_TEXT_OPTIONS_CLIP);
    round(41, 280, 158, 4, 2, 0x8999a5, .45f);
    if (secondary >= 0)
        round(41, 280, 158 * static_cast<float>(std::clamp(secondary, 0., 100.)) / 100, 4, 2, 0x6cc5b5);
    round(27, 285, 186, 15, 7, 0x152330, .94f);
    color(t.text_color);
    dc_->DrawTextLayout(D2D1::Point2F(28, 285), caption_layout_.Get(), brush_.Get(),
                        D2D1_DRAW_TEXT_OPTIONS_CLIP);
    hr(dc_->EndDraw());
    if (low_memory_) {
        layered_present(layered ? layered : window_);
        return;
    }
    if (layered)
        layered_present(layered);
    hr(swap_->Present(1, 0));
}
void Renderer::layered_present(HWND window) {
    if (!low_memory_) {
        ComPtr<ID3D11Texture2D> buffer;
        hr(swap_->GetBuffer(0, IID_PPV_ARGS(&buffer)));
        if (!staging_) {
            D3D11_TEXTURE2D_DESC desc{};
            buffer->GetDesc(&desc);
            desc.Usage = D3D11_USAGE_STAGING;
            desc.BindFlags = 0;
            desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
            desc.MiscFlags = 0;
            hr(d3d_->CreateTexture2D(&desc, nullptr, &staging_));
            allocate_dib();
        }
        immediate_->CopyResource(staging_.Get(), buffer.Get());
        D3D11_MAPPED_SUBRESOURCE map{};
        hr(immediate_->Map(staging_.Get(), 0, D3D11_MAP_READ, 0, &map));
        for (UINT y = 0; y < height_; y++)
            memcpy(static_cast<char *>(pixels_) + static_cast<size_t>(y) * width_ * 4,
                   static_cast<char *>(map.pData) + static_cast<size_t>(y) * map.RowPitch, width_ * 4);
        immediate_->Unmap(staging_.Get(), 0);
    }
    RECT rc{};
    GetWindowRect(window, &rc);
    POINT destination{rc.left, rc.top}, origin{};
    SIZE size{static_cast<LONG>(width_), static_cast<LONG>(height_)};
    BLENDFUNCTION blend{AC_SRC_OVER, 0, 255, AC_SRC_ALPHA};
    if (!UpdateLayeredWindow(window, nullptr, &destination, &size, memory_dc_, &origin, 0, &blend, ULW_ALPHA))
        throw std::runtime_error("Layered presentation failed");
}
void Renderer::allocate_dib() {
    memory_dc_ = CreateCompatibleDC(nullptr);
    BITMAPINFO info{};
    info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    info.bmiHeader.biWidth = width_;
    info.bmiHeader.biHeight = -static_cast<LONG>(height_);
    info.bmiHeader.biPlanes = 1;
    info.bmiHeader.biBitCount = 32;
    info.bmiHeader.biCompression = BI_RGB;
    dib_ = CreateDIBSection(memory_dc_, &info, DIB_RGB_COLORS, &pixels_, nullptr, 0);
    if (!memory_dc_ || !dib_) {
        release_readback();
        throw std::runtime_error("Layered surface allocation failed");
    }
    old_dib_ = SelectObject(memory_dc_, dib_);
}
} // namespace beer
