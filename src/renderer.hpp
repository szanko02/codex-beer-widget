#pragma once
#include "theme.hpp"
#include <d2d1_1.h>
#include <d3d11.h>
#include <dcomp.h>
#include <dwrite_3.h>
#include <dxgi1_2.h>
#include <string>
#include <windows.h>
#include <wrl/client.h>

namespace beer {
using Microsoft::WRL::ComPtr;
class Renderer {
  public:
    explicit Renderer(HWND window, bool low_memory = false);
    void resize(UINT width, UINT height);
    void draw(const Theme &theme, double remaining, double secondary, const std::wstring &label,
              const std::wstring &caption, double phase, HWND layered_window = nullptr);
    bool software() const { return software_; }

  private:
    HWND window_{};
    UINT width_ = 1, height_ = 1;
    bool software_{};
    bool low_memory_{};
    ComPtr<ID3D11Device> d3d_;
    ComPtr<ID3D11DeviceContext> immediate_;
    ComPtr<IDXGISwapChain1> swap_;
    ComPtr<ID2D1Factory1> factory_;
    ComPtr<ID2D1Device> device_;
    ComPtr<ID2D1RenderTarget> dc_;
    ComPtr<ID2D1DeviceContext> gpu_context_;
    ComPtr<ID2D1DCRenderTarget> cpu_target_;
    ComPtr<ID2D1Layer> clip_layer_;
    ComPtr<ID2D1Bitmap1> bitmap_;
    ComPtr<IDCompositionDevice> composition_;
    ComPtr<IDCompositionTarget> target_;
    ComPtr<IDCompositionVisual> root_;
    ComPtr<IDWriteFactory> write_;
    ComPtr<IDWriteFontCollection1> fonts_;
    ComPtr<IDWriteTextFormat> text_;
    ComPtr<IDWriteTextFormat> small_text_;
    ComPtr<IDWriteTextLayout> label_layout_, caption_layout_;
    std::wstring cached_label_, cached_caption_;
    ComPtr<ID2D1SolidColorBrush> brush_;
    ComPtr<ID2D1RoundedRectangleGeometry> inside_;
    ComPtr<ID2D1LinearGradientBrush> beer_, glass_;
    Theme cached_{};
    bool cached_valid_ = false;
    ComPtr<ID3D11Texture2D> staging_;
    HDC memory_dc_{};
    HBITMAP dib_{};
    HGDIOBJ old_dib_{};
    void *pixels_{};
    void update_theme(const Theme &theme);
    void layered_present(HWND window);
    void release_readback();
    void allocate_dib();
    void color(uint32_t rgb, float alpha = 1);
    void round(float x, float y, float w, float h, float r, uint32_t rgb, float alpha = 1);

  public:
    ~Renderer();
};
} // namespace beer
