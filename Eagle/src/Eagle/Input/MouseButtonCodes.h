#pragma once

namespace Eagle
{
	using uint16_t = unsigned short int;

	namespace Mouse
	{
		using MouseButton = uint16_t;
		
		enum : MouseButton
		{
			// From glfw3.h
			Button0 = 0,
			Button1 = 1,
			Button2 = 2,
			Button3 = 3,
			Button4 = 4,
			Button5 = 5,
			Button6 = 6,
			Button7 = 7,

			ButtonLast = Button7,
			ButtonLeft = Button0,
			ButtonRight = Button1,
			ButtonMiddle = Button2
		};
	}
}

#define EG_MOUSE_BUTTON_0      ::Eagle::Mouse::Button0
#define EG_MOUSE_BUTTON_1      ::Eagle::Mouse::Button1
#define EG_MOUSE_BUTTON_2      ::Eagle::Mouse::Button2
#define EG_MOUSE_BUTTON_3      ::Eagle::Mouse::Button3
#define EG_MOUSE_BUTTON_4      ::Eagle::Mouse::Button4
#define EG_MOUSE_BUTTON_5      ::Eagle::Mouse::Button5
#define EG_MOUSE_BUTTON_6      ::Eagle::Mouse::Button6
#define EG_MOUSE_BUTTON_7      ::Eagle::Mouse::Button7
#define EG_MOUSE_BUTTON_LAST   ::Eagle::Mouse::ButtonLast
#define EG_MOUSE_BUTTON_LEFT   ::Eagle::Mouse::ButtonLeft
#define EG_MOUSE_BUTTON_RIGHT  ::Eagle::Mouse::ButtonRight
#define EG_MOUSE_BUTTON_MIDDLE ::Eagle::Mouse::ButtonMiddle
