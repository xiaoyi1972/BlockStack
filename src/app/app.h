#pragma once
#include "resource.h"
#include "engine.h"
#include <iostream>
#include <future>

namespace chrome {
	class Application
	{
	public:

		struct menu_item{
			std::string name;
			measure::rectangle<float> rect;
			inline static struct {
				const char* font_family = u8"MSYHL"_C; //u8"Microsoft Sans Serif";
				float font_size = 14.0f;
				float space = 10.f;
				int hover_index = -1;
				int click_index = -1;
				int width = 0;
				int height = 0;
			}info;
		};

		HMENU hMenu_1{};

		std::vector<menu_item> menus;

		Application(Application const&) = delete;
		Application(Application&&) = delete;

		Application& operator=(Application const&) = delete;
		Application& operator=(Application&&) = delete;

		~Application() = default;


		void init_menus() {
			std::string titles[] = { u8"模式(M)"_S, u8"帮助(H)"_S };
			auto x = 5.f;
			for (auto& v : titles) {
				menus.push_back({ 
					v, engine._renderer->text_layout_boundary(v, 
					measure::point<float>(0, 0), menu_item::info.font_family, menu_item::info.font_size)
					});
				menus.back().rect.origin.x = x;
				x += menus.back().rect.dimension.width + menu_item::info.space;
			}
			if (!menus.empty()) {
				menu_item::info.width = menus.front().rect.dimension.width;
				menu_item::info.height = menus.front().rect.dimension.height;
			}
			std::cout << "hao" << "\n";
		}

		Application() {
			//auto frame = measure::rectangle{ 100.0f, 100.0f, 1280.f, 720.f };
			auto frame = measure::rectangle{ 100.0f, 100.0f, RESOLUTION_X , RESOLUTION_Y }; //window rect size
			Initiallize("Chrome management", frame);
			show_window();

			init_menus();
			//text_layout_boundary();


		}

		auto execute() -> int {

			RunMessageLoop();
			return 1;

			MSG message_structure{};

			while (message_structure.message != WM_QUIT)
				if (PeekMessageW(&message_structure, nullptr, 0, 0, PM_REMOVE)) {
					TranslateMessage(&message_structure);
					DispatchMessageW(&message_structure);
				}

			return static_cast<int>(message_structure.wParam);

		}

		HRESULT Initiallize(std::string title, measure::rectangle<float> const& frame) {
			_title = std::move(title);

			WNDCLASSEX window_class{

				sizeof(WNDCLASSEX), CS_VREDRAW | CS_HREDRAW, WndProc, 0, sizeof(this),
				GetModuleHandleW(nullptr), nullptr, LoadCursor(nullptr, IDC_ARROW),
				(HBRUSH)GetStockObject(BLACK_BRUSH), nullptr, L"BasicWindow", nullptr

			};

			RegisterClassEx(&window_class);

			auto wide_title = utility::convert_utf8_to_utf16(_title);

			//Menu
			//HMENU hMenu = CreateMenu();
			hMenu_1 = CreatePopupMenu();
			AppendMenu(hMenu_1, 0, 2, L"对战");
			AppendMenu(hMenu_1, 0, 3, L"竞速");
			AppendMenu(hMenu_1, 0, 4, L"挖掘");

			//AppendMenu(hMenu, MF_POPUP, (UINT)hMenu_1, L"模式");
			MENUITEMINFO mii = { 0 };
			mii.cbSize = sizeof(MENUITEMINFO);
			mii.fMask = MIIM_STATE;
			GetMenuItemInfo(hMenu_1, 2, false, &mii);
			mii.fState |= MFS_CHECKED;
			SetMenuItemInfo(hMenu_1, 2, false, &mii);

			_system_window_handle = CreateWindowEx(
				WS_EX_NOREDIRECTIONBITMAP, window_class.lpszClassName, wide_title.c_str(),
				WS_OVERLAPPEDWINDOW, 10, 10, 100, 100, nullptr, nullptr, window_class.hInstance, this
			);

			if (_system_window_handle == nullptr) throw std::runtime_error { "Failed to create a window." };

			ImmAssociateContext(_system_window_handle, NULL); //ban imme

			SetProcessDpiAwareness(PROCESS_SYSTEM_DPI_AWARE);
			auto dpi = GetDpiForWindow(_system_window_handle);
			_user_scaling = static_cast<float>(dpi) / 96.0f;

			auto [origin, dimension] = frame;
			dimension *= _user_scaling;

			SetWindowPos(
				_system_window_handle, nullptr, static_cast<int>(origin.x), static_cast<int>(origin.y),
				static_cast<int>(dimension.width), static_cast<int>(dimension.height), SWP_FRAMECHANGED
			);

			auto caption_height = gui::helper::compute_standard_caption_height_for_window(_system_window_handle);
			_margins = { 0, 0, static_cast<int>(caption_height), 0 };

			auto hr = DwmExtendFrameIntoClientArea(_system_window_handle, &_margins);
			if (FAILED(hr)) throw std::runtime_error { "DWM failed to extend frame into the client area." };

			_client_area_offset_dip = static_cast<float>(_margins.cyTopHeight) / _user_scaling;

			//_tab_raster = std::make_unique<resource::image>("media/tab_raster.png");
			//_new_tab_symbol = std::make_unique<resource::image>("media/new_tab_symbol.png");

			engine._renderer = std::make_unique<graphics::renderer>();
			engine._renderer->attach_to_window(_system_window_handle);
			engine.InitializeD2D();


		}

		static LRESULT CALLBACK WndProc(HWND window_handle, UINT message, WPARAM wparam, LPARAM lparam)
		{
			LRESULT result;
			// Ask whether DWM would like to process the incoming message (to handle the caption parts)
			auto dwm_has_processed = DwmDefWindowProc(window_handle, message, wparam, lparam, &result);
			if (dwm_has_processed) return result;

			//auto window = reinterpret_cast<chrome::gui::window*>(GetWindowLongPtr(window_handle, GWLP_USERDATA));
			Application* pThis = nullptr;

			if (message == WM_NCCREATE)
			{
				pThis = static_cast<Application*>(reinterpret_cast<CREATESTRUCT*>(lparam)->lpCreateParams);

				SetLastError(0);
				if (!SetWindowLongPtr(window_handle, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(pThis)))
				{
					if (GetLastError() != 0)
						return FALSE;
				}
			}
			else
			{
				pThis = reinterpret_cast<Application*>(GetWindowLongPtr(window_handle, GWLP_USERDATA));
			}

			if (message == WM_CREATE) {

				auto create_struct = reinterpret_cast<CREATESTRUCT*>(lparam);
				SetWindowLongPtrW(window_handle, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(create_struct->lpCreateParams));

				// We need to trigger recompute of the window and client area.
				SetWindowPos(window_handle, nullptr, 0, 0, 0, 0, SWP_FRAMECHANGED | SWP_NOMOVE | SWP_NOSIZE);

			}

			// Extends the client area all around (returning 0 when wparam is TRUE)
			else if (message == WM_NCCALCSIZE) {

				auto client_area_needs_calculating = static_cast<bool>(wparam);

				if (client_area_needs_calculating) {

					auto parameters = reinterpret_cast<NCCALCSIZE_PARAMS*>(lparam);

					auto& requested_client_area = parameters->rgrc[0];
					requested_client_area.right -= GetSystemMetrics(SM_CXFRAME) + GetSystemMetrics(SM_CXPADDEDBORDER);
					requested_client_area.left += GetSystemMetrics(SM_CXFRAME) + GetSystemMetrics(SM_CXPADDEDBORDER);
					requested_client_area.bottom -= GetSystemMetrics(SM_CYFRAME) + GetSystemMetrics(SM_CXPADDEDBORDER);

					return 0;

				}

			}

			// Determine whether the cursor is near interactive points of the window
			else if ((message == WM_NCHITTEST)) {

				result = gui::helper::compute_sector_of_window(window_handle, wparam, lparam, pThis->_margins.cyTopHeight);
				if (result != HTNOWHERE) return result;

			}

			/*else if (message == WM_PAINT) {
				pThis->begin_paint();
				pThis->engine._renderer->fill_rectangle(measure::rectangle{0.f, 0.f, 100.f, 100.f}, measure::color(1.f, 0.f, 0.f));;
				//pThis->engine._renderer->fill_rectangle(measure::rectangle{0.f, 0.f, 100.f, 100.f}, measure::color(1.f, 1.f, 0.f));;
				pThis->end_paint();
			}*/

			/*else if (message == WM_GETMINMAXINFO) {
				MINMAXINFO* mminfo;
				mminfo = reinterpret_cast<MINMAXINFO*>(lparam);
				mminfo->ptMaxPosition.x = 0;
				mminfo->ptMaxPosition.y = 0;
				mminfo->ptMaxSize.x = 700;
				mminfo->ptMaxSize.y = 700;

			}*/

			else if (message == WM_SIZE) {
				pThis->engine.handle_resize();
			}

			else if (message == WM_DISPLAYCHANGE)
			{
				InvalidateRect(window_handle, NULL, FALSE);
			}

			else if(message ==  WM_KEYDOWN)
			{
				pThis->engine.KeyDown(wparam);
			}

			else if(message == WM_KEYUP)
			{
				pThis->engine.KeyUp(wparam);
			}

			else if(message == WM_MOUSEMOVE)
			{
				menu_item::info.hover_index = -1;
				menu_item::info.click_index = -1;
				pThis->engine.MousePosition(GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam));
			}

			else if (message == WM_LBUTTONUP)
			{
				pThis->engine.MouseButtonUp(true, false);
			}

			else if (message == WM_LBUTTONDOWN)
			{
				pThis->engine.MouseButtonDown(true, false);
			}

			else if (message == WM_RBUTTONUP)
			{
				pThis->engine.MouseButtonUp(false, true);
			}

			else if (message == WM_RBUTTONDOWN)
			{
				pThis->engine.MouseButtonDown(false, true);
			}

			else if(message == WM_COMMAND)
			{
				int arr[]{ 2,3,4 };
				int currrent_id = wparam;
				for (auto i : arr) {
					MENUITEMINFO mii = { 0 };
					mii.cbSize = sizeof(MENUITEMINFO);
					mii.fMask = MIIM_STATE;
					GetMenuItemInfo(pThis->hMenu_1, i, false, &mii);
					mii.fState = 0;
					if (i == currrent_id)
						mii.fState |= MFS_CHECKED;
					SetMenuItemInfo(pThis->hMenu_1, i, false, &mii);
				}
				pThis->engine.Command(wparam);
			}

			else if (message == WM_DESTROY) {
				PostQuitMessage(0);
			}

			else if (message == WM_NCMOUSEMOVE) {
				POINT pt{ GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam) };
				ScreenToClient(window_handle, &pt);

				auto x = static_cast<float>(pt.x) / pThis->_user_scaling;
				auto y = static_cast<float>(pt.y) / pThis->_user_scaling;

				//v.rect.origin.x + 5, -_client_area_offset_dip + 5.f
//				pt.x -= 5, pt.y -= (-_client_area_offset_dip + 5.f);
				menu_item::info.hover_index = -1;
				{
					auto i = 0;
					for (auto& v : pThis->menus) {
						if (x >= v.rect.origin.x &&
							x <= v.rect.origin.x + v.rect.dimension.width &&
							y >= v.rect.origin.y &&
							y <= v.rect.origin.y + v.rect.dimension.height) {
							menu_item::info.hover_index = i;
						}
						i++;
					}
				}

				//if (pt.x < 60 && pt.y < 30) pThis->entered = true;
				//else pThis->entered = false;

				//std::cout << "2-> x:" << pt.x << " y:" << pt.y << "\n";

			}

			else if (message == WM_NCLBUTTONDOWN) {
				POINT pt{ GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam) };
				ScreenToClient(window_handle, &pt);
				//std::cout << "click at x,y for (" << pt.x << "," << pt.y << ")\n";

				auto x = static_cast<float>(pt.x) / pThis->_user_scaling;
				auto y = static_cast<float>(pt.y) / pThis->_user_scaling;

				auto pt1 = pt;

				menu_item::info.click_index = -1;
				{
					auto i = 0;
					for (auto& v : pThis->menus) {
						if (x >= v.rect.origin.x &&
							x <= v.rect.origin.x + v.rect.dimension.width &&
							y >= v.rect.origin.y &&
							y <= v.rect.origin.y + v.rect.dimension.height) {
							menu_item::info.click_index = i;
							pt1.x = v.rect.origin.x;
							pt1.y =  v.rect.origin.y + v.rect.dimension.height + 5;
						}
						i++;
					}
				}


				if (menu_item::info.click_index != -1) {
					pt1.x *= pThis->_user_scaling;
					pt1.y *= pThis->_user_scaling;
					ClientToScreen(window_handle, &pt1);
					TrackPopupMenuEx(pThis->hMenu_1, TPM_LEFTALIGN | TPM_TOPALIGN, pt1.x, pt1.y, window_handle, nullptr);
				}

			}

			return DefWindowProc(window_handle, message, wparam, lparam);
		}

		void begin_paint() {
			auto &_renderer = engine._renderer;
			_renderer->begin_draw();

			RECT client_rectangle;
			GetClientRect(_system_window_handle, &client_rectangle);

			measure::rectangle<float> client_area {
				0.0f, 0.0f,
					static_cast<float>(client_rectangle.right),
					static_cast<float>(client_rectangle.bottom - _margins.cyTopHeight)
			};

			client_area *= 1.0f / _user_scaling;

			_renderer->push_transform(D2D1::Matrix3x2F::Translation(0.0f, _client_area_offset_dip));

			// Paint the whole client area into our baseline color.
			_renderer->fill_rectangle(client_area, measure::color{ 0.96f, 0.96f, 0.96f });

			//draw menus

			if (menu_item::info.hover_index != -1) {
				auto& rect = menus[menu_item::info.hover_index].rect;
				_renderer->fill_rectangle(
					measure::rectangle<float>(
						rect.origin.x, - _client_area_offset_dip + 5.f,
						rect.dimension.width, rect.dimension.height
					), 
					measure::color(0.6f, 0.6f, 0.6f, .4f));;
			}

			for (auto& v : menus) {
				_renderer->draw_text(
					v.name, measure::point<float>{ v.rect.origin.x,  - _client_area_offset_dip + 5.f},
					menu_item::info.font_family, menu_item::info.font_size, measure::color{ 0.f, 0.f, 0.f, 1.0f }
				);
				/*_renderer->draw_rectangle(
					measure::rectangle<float>(v.rect.origin.x,v.rect.origin.y - _client_area_offset_dip + 5.f,
						v.rect.dimension.width, v.rect.dimension.height),
						measure::color{ 1.f, 0.f, 0.f, 1.0f }
				);*/
			}

			_renderer->push_transform(D2D1::Matrix3x2F::Translation(0.0f, -_client_area_offset_dip));

		}

		void end_paint() {
			auto& _renderer = engine._renderer;
			_renderer->end_draw();
		}

		void RunMessageLoop()
		{
			MSG msg;
			std::chrono::steady_clock::time_point begin = std::chrono::steady_clock::now();
			std::chrono::steady_clock::time_point end = std::chrono::steady_clock::now();
			int frames = 0;
			double framesTime = 0, lastElapsedTime = 0;
			boolean running = true;

			while (running)
			{
				end = std::chrono::steady_clock::now();
				std::chrono::duration<double> deltaPoint = end - begin;
				double elapsed_secs = deltaPoint.count();
				begin = end;
				framesTime += elapsed_secs; // 统计FPS
				while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) // 处理消息和用户输入
				{
					TranslateMessage(&msg);
					DispatchMessage(&msg);
					if (msg.message == WM_QUIT)
						running = false;
				}
				auto delta = framesTime - lastElapsedTime;
				engine.Logic(elapsed_secs); // 游戏逻辑call
				if (engine.Draw(this, delta) == true) { // 绘图call
					lastElapsedTime = framesTime;
					frames++;
				}
				if (framesTime > 1) {
					WCHAR fpsText[32];
					swprintf(fpsText, 32, L"Game: %d FPS", frames);
					SetWindowText(m_hwnd, fpsText);
					frames = lastElapsedTime = framesTime = 0;
				}
			}
		}

		auto show_window() -> void {
			ShowWindow(_system_window_handle, SW_SHOW);
		}

		auto hide_window() -> void {
			ShowWindow(_system_window_handle, SW_HIDE);
		}

	private:
		HWND _system_window_handle = nullptr;
		MARGINS _margins{};
		std::string _title;
		float _user_scaling = 96.0f;
		float _client_area_offset_dip = 0.0f;
		HWND m_hwnd;
		Engine engine;
		static FILE* stream;

		bool entered = false;
	};

}