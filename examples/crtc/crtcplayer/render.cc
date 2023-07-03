template <typename T>
class AutoLock {
public:
       explicit AutoLock(T* obj) : obj_(obj) { obj_->Lock(); }
       ~AutoLock() { obj_->Unlock(); }
protected:
       T* obj_;
};

class VideoRenderer : public rtc::VideoSinkInterface<webrtc::VideoFrame> {
public:
       VideoRenderer(HWND wnd,
              int width,
              int height,
              webrtc::VideoTrackInterface* track_to_render)
              : wnd_(wnd), rendered_track_(track_to_render)
       {
              ::InitializeCriticalSection(&buffer_lock_);
              ZeroMemory(&bmi_, sizeof(bmi_));
              bmi_.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
              bmi_.bmiHeader.biPlanes = 1;
              bmi_.bmiHeader.biBitCount = 32;
              bmi_.bmiHeader.biCompression = BI_RGB;
              bmi_.bmiHeader.biWidth = width;
              bmi_.bmiHeader.biHeight = -height;
              bmi_.bmiHeader.biSizeImage =
                     width * height * (bmi_.bmiHeader.biBitCount >> 3);
              rendered_track_->AddOrUpdateSink(this, rtc::VideoSinkWants());
       }
       virtual ~VideoRenderer()
       {
              rendered_track_->RemoveSink(this);
              ::DeleteCriticalSection(&buffer_lock_);
       }
       void Lock() { ::EnterCriticalSection(&buffer_lock_); }
       void Unlock() { ::LeaveCriticalSection(&buffer_lock_); }
       // VideoSinkInterface implementation
       void OnFrame(const webrtc::VideoFrame& video_frame) override
       {
              {
                     AutoLock<VideoRenderer> lock(this);
                     rtc::scoped_refptr<webrtc::I420BufferInterface> buffer(
                           video_frame.video_frame_buffer()->ToI420());
                     if (video_frame.rotation() != webrtc::kVideoRotation_0) {
                           buffer = webrtc::I420Buffer::Rotate(*buffer,  video_frame.rotation());
                     }
                     SetSize(buffer->width(), buffer->height());
                     RTC_DCHECK(image_.get() != NULL);
                     libyuv::I420ToARGB(buffer->DataY(), buffer->StrideY(),  buffer->DataU(),
                           buffer->StrideU(), buffer->DataV(), buffer->StrideV(),
                           image_.get(),
                           bmi_.bmiHeader.biWidth * bmi_.bmiHeader.biBitCount /  8,
                           buffer->width(), buffer->height());
              }
              InvalidateRect(wnd_, NULL, /*TRUE*/FALSE);
       }
       const BITMAPINFO& bmi() const { return bmi_; }
       const uint8_t* image() const { return image_.get(); }

protected:
       void SetSize(int width, int height)
       {
              AutoLock<VideoRenderer> lock(this);
              if (width == bmi_.bmiHeader.biWidth && height ==  bmi_.bmiHeader.biHeight) {
                     return;
              }
              bmi_.bmiHeader.biWidth = width;
              bmi_.bmiHeader.biHeight = -height;
              bmi_.bmiHeader.biSizeImage =
                     width * height * (bmi_.bmiHeader.biBitCount >> 3);
              image_.reset(new uint8_t[bmi_.bmiHeader.biSizeImage]);
       }
       enum {
              SET_SIZE,
              RENDER_FRAME,
       };
       HWND wnd_;
       BITMAPINFO bmi_;
       std::unique_ptr<uint8_t[]> image_;
       CRITICAL_SECTION buffer_lock_;
       rtc::scoped_refptr<webrtc::VideoTrackInterface> rendered_track_;
};

std::unique_ptr<VideoRenderer> local_renderer_;
std::unique_ptr<VideoRenderer> remote_renderer_;

HFONT GetDefaultFont()
{
       static HFONT font =  reinterpret_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT));
       return font;
}

//WM_PAINT消息响应函数
void OnPaint()
{
       if (gWnd == nullptr) {
              return;
       }
       PAINTSTRUCT ps;
       ::BeginPaint(gWnd, &ps);
       RECT rc;
       ::GetClientRect(gWnd, &rc);
       VideoRenderer* local_renderer = local_renderer_.get();
       VideoRenderer* remote_renderer = remote_renderer_.get();
       if (remote_renderer && local_renderer) {
              AutoLock<VideoRenderer> local_lock(local_renderer);
              AutoLock<VideoRenderer> remote_lock(remote_renderer);
              const BITMAPINFO& bmi = remote_renderer->bmi();
              int height = abs(bmi.bmiHeader.biHeight);
              int width = bmi.bmiHeader.biWidth;
              const uint8_t* image = remote_renderer->image();
              if (image != NULL) {
                     HDC dc_mem = ::CreateCompatibleDC(ps.hdc);
                     ::SetStretchBltMode(dc_mem, HALFTONE);
                     // Set the map mode so that the ratio will be maintained for  us.
                     HDC all_dc[] = { ps.hdc, dc_mem };
                     for (size_t i = 0; i < arraysize(all_dc); ++i) {
                           SetMapMode(all_dc[i], MM_ISOTROPIC);
                           SetWindowExtEx(all_dc[i], width, height, NULL);
                           SetViewportExtEx(all_dc[i], rc.right, rc.bottom,  NULL);
                     }
                     HBITMAP bmp_mem = ::CreateCompatibleBitmap(ps.hdc, rc.right,  rc.bottom);
                     HGDIOBJ bmp_old = ::SelectObject(dc_mem, bmp_mem);
                     POINT logical_area = { rc.right, rc.bottom };
                     DPtoLP(ps.hdc, &logical_area, 1);
                     HBRUSH brush = ::CreateSolidBrush(RGB(0, 0, 0));
                     RECT logical_rect = { 0, 0, logical_area.x, logical_area.y };
                     ::FillRect(dc_mem, &logical_rect, brush);
                     ::DeleteObject(brush);
                     int x = (logical_area.x / 2) - (width / 2);
                     int y = (logical_area.y / 2) - (height / 2);
                     StretchDIBits(dc_mem, x, y, width, height, 0, 0, width,  height, image,
                           &bmi, DIB_RGB_COLORS, SRCCOPY);
                     if ((rc.right - rc.left) > 200 && (rc.bottom - rc.top) > 200)  {
                           const BITMAPINFO& bmi = local_renderer->bmi();
                           image = local_renderer->image();
                           int thumb_width = bmi.bmiHeader.biWidth / 4;
                           int thumb_height = abs(bmi.bmiHeader.biHeight) / 4;
                           StretchDIBits(dc_mem, logical_area.x - thumb_width -  10,
                                  logical_area.y - thumb_height - 10, thumb_width,
                                  thumb_height, 0, 0, bmi.bmiHeader.biWidth,
                                  -bmi.bmiHeader.biHeight, image, &bmi,  DIB_RGB_COLORS,
                                  SRCCOPY);
                     }
                     BitBlt(ps.hdc, 0, 0, logical_area.x, logical_area.y, dc_mem,  0, 0,
                           SRCCOPY);
                     // Cleanup.
                     ::SelectObject(dc_mem, bmp_old);
                     ::DeleteObject(bmp_mem);
                     ::DeleteDC(dc_mem);
              }
              else {
                     // We're still waiting for the video stream to be  initialized.
                     HBRUSH brush = ::CreateSolidBrush(RGB(0, 0, 0));
                     ::FillRect(ps.hdc, &rc, brush);
                     ::DeleteObject(brush);
                     HGDIOBJ old_font = ::SelectObject(ps.hdc, GetDefaultFont());
                     ::SetTextColor(ps.hdc, RGB(0xff, 0xff, 0xff));
                     ::SetBkMode(ps.hdc, TRANSPARENT);
                     std::string text("Connecting... ");
                     if (!local_renderer->image()) {
                           text += "(no video streams either way)";
                     }
                     else {
                           text += "(no incoming video)";
                     }
                     ::DrawTextA(ps.hdc, text.c_str(), -1, &rc,
                           DT_SINGLELINE | DT_CENTER | DT_VCENTER);
                     ::SelectObject(ps.hdc, old_font);
              }
       }
       else {
              HBRUSH brush = ::CreateSolidBrush(::GetSysColor(COLOR_WINDOW));
              ::FillRect(ps.hdc, &rc, brush);
              ::DeleteObject(brush);
       }
       ::EndPaint(gWnd, &ps);
}
