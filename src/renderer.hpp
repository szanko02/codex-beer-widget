#pragma once
#include "theme.hpp"
#include <windows.h>
#include <d2d1_1.h>
#include <d3d11.h>
#include <dxgi1_2.h>
#include <dcomp.h>
#include <dwrite.h>
#include <wrl/client.h>
#include <string>

namespace beer {
using Microsoft::WRL::ComPtr;
class Renderer {
public:
    explicit Renderer(HWND window);
    void resize(UINT width, UINT height);
    void draw(const Theme& theme, double remaining, double secondary, const std::wstring& label,
              const std::wstring& caption, double phase, HWND layered_window = nullptr);
    bool software() const { return software_; }
private:
    HWND window_{}; UINT width_ = 1, height_ = 1; bool software_{};
    ComPtr<ID3D11Device> d3d_;
    ComPtr<ID3D11DeviceContext> immediate_;
    ComPtr<IDXGISwapChain1> swap_;
    ComPtr<ID2D1Factory1> factory_;
    ComPtr<ID2D1Device> device_;
    ComPtr<ID2D1DeviceContext> dc_;
    ComPtr<ID2D1Bitmap1> bitmap_;
    ComPtr<IDCompositionDevice> composition_;
    ComPtr<IDCompositionTarget> target_;
    ComPtr<IDCompositionVisual> root_;
    ComPtr<IDWriteFactory> write_;
    ComPtr<IDWriteTextFormat> text_;
    ComPtr<IDWriteTextFormat> small_text_;
    ComPtr<ID2D1SolidColorBrush> brush_;
    ComPtr<ID2D1RoundedRectangleGeometry> inside_;
    ComPtr<ID2D1LinearGradientBrush> beer_, glass_;
    Theme cached_{}; bool cached_valid_ = false;
    ComPtr<ID3D11Texture2D> staging_;
    HDC memory_dc_{}; HBITMAP dib_{}; HGDIOBJ old_dib_{}; void* pixels_{};
    void update_theme(const Theme& theme);
    void layered_present(HWND window);
    void release_readback();
    void color(uint32_t rgb, float alpha = 1);
    void round(float x, float y, float w, float h, float r, uint32_t rgb, float alpha = 1);
    void text(const std::wstring& value, D2D1_RECT_F bounds, uint32_t rgb);
public:
    ~Renderer();
};
}
