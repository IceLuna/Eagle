#pragma once

namespace Eagle
{
	using uint16_t = unsigned short int;

	namespace Key
	{
		using KeyCode = uint16_t;
		
		enum : KeyCode
		{
			// From glfw3.h
			Space = 32,
			Apostrophe = 39, /* ' */
			Comma = 44, /* , */
			Minus = 45, /* - */
			Period = 46, /* . */
			Slash = 47, /* / */

			D0 = 48, /* 0 */
			D1 = 49, /* 1 */
			D2 = 50, /* 2 */
			D3 = 51, /* 3 */
			D4 = 52, /* 4 */
			D5 = 53, /* 5 */
			D6 = 54, /* 6 */
			D7 = 55, /* 7 */
			D8 = 56, /* 8 */
			D9 = 57, /* 9 */

			Semicolon = 59, /* ; */
			Equal = 61, /* = */

			A = 65,
			B = 66,
			C = 67,
			D = 68,
			E = 69,
			F = 70,
			G = 71,
			H = 72,
			I = 73,
			J = 74,
			K = 75,
			L = 76,
			M = 77,
			N = 78,
			O = 79,
			P = 80,
			Q = 81,
			R = 82,
			S = 83,
			T = 84,
			U = 85,
			V = 86,
			W = 87,
			X = 88,
			Y = 89,
			Z = 90,

			LeftBracket = 91,  /* [ */
			Backslash = 92,  /* \ */
			RightBracket = 93,  /* ] */
			GraveAccent = 96,  /* ` */

			World1 = 161, /* non-US #1 */
			World2 = 162, /* non-US #2 */

			/* Function keys */
			Escape = 256,
			Enter = 257,
			Tab = 258,
			Backspace = 259,
			Insert = 260,
			Delete = 261,
			Right = 262,
			Left = 263,
			Down = 264,
			Up = 265,
			PageUp = 266,
			PageDown = 267,
			Home = 268,
			End = 269,
			CapsLock = 280,
			ScrollLock = 281,
			NumLock = 282,
			PrintScreen = 283,
			Pause = 284,
			F1 = 290,
			F2 = 291,
			F3 = 292,
			F4 = 293,
			F5 = 294,
			F6 = 295,
			F7 = 296,
			F8 = 297,
			F9 = 298,
			F10 = 299,
			F11 = 300,
			F12 = 301,
			F13 = 302,
			F14 = 303,
			F15 = 304,
			F16 = 305,
			F17 = 306,
			F18 = 307,
			F19 = 308,
			F20 = 309,
			F21 = 310,
			F22 = 311,
			F23 = 312,
			F24 = 313,
			F25 = 314,

			/* Keypad */
			KP0 = 320,
			KP1 = 321,
			KP2 = 322,
			KP3 = 323,
			KP4 = 324,
			KP5 = 325,
			KP6 = 326,
			KP7 = 327,
			KP8 = 328,
			KP9 = 329,
			KPDecimal = 330,
			KPDivide = 331,
			KPMultiply = 332,
			KPSubtract = 333,
			KPAdd = 334,
			KPEnter = 335,
			KPEqual = 336,

			LeftShift = 340,
			LeftControl = 341,
			LeftAlt = 342,
			LeftSuper = 343,
			RightShift = 344,
			RightControl = 345,
			RightAlt = 346,
			RightSuper = 347,
			Menu = 348
		};
	}
}

#define EG_KEY_SPACE           ::Eagle::Key::Space
#define EG_KEY_APOSTROPHE      ::Eagle::Key::Apostrophe    /* ' */
#define EG_KEY_COMMA           ::Eagle::Key::Comma         /* , */
#define EG_KEY_MINUS           ::Eagle::Key::Minus         /* - */
#define EG_KEY_PERIOD          ::Eagle::Key::Period        /* . */
#define EG_KEY_SLASH           ::Eagle::Key::Slash         /* / */
#define EG_KEY_0               ::Eagle::Key::D0
#define EG_KEY_1               ::Eagle::Key::D1
#define EG_KEY_2               ::Eagle::Key::D2
#define EG_KEY_3               ::Eagle::Key::D3
#define EG_KEY_4               ::Eagle::Key::D4
#define EG_KEY_5               ::Eagle::Key::D5
#define EG_KEY_6               ::Eagle::Key::D6
#define EG_KEY_7               ::Eagle::Key::D7
#define EG_KEY_8               ::Eagle::Key::D8
#define EG_KEY_9               ::Eagle::Key::D9
#define EG_KEY_SEMICOLON       ::Eagle::Key::Semicolon     /* ; */
#define EG_KEY_EQUAL           ::Eagle::Key::Equal         /* = */
#define EG_KEY_A               ::Eagle::Key::A
#define EG_KEY_B               ::Eagle::Key::B
#define EG_KEY_C               ::Eagle::Key::C
#define EG_KEY_D               ::Eagle::Key::D
#define EG_KEY_E               ::Eagle::Key::E
#define EG_KEY_F               ::Eagle::Key::F
#define EG_KEY_G               ::Eagle::Key::G
#define EG_KEY_H               ::Eagle::Key::H
#define EG_KEY_I               ::Eagle::Key::I
#define EG_KEY_J               ::Eagle::Key::J
#define EG_KEY_K               ::Eagle::Key::K
#define EG_KEY_L               ::Eagle::Key::L
#define EG_KEY_M               ::Eagle::Key::M
#define EG_KEY_N               ::Eagle::Key::N
#define EG_KEY_O               ::Eagle::Key::O
#define EG_KEY_P               ::Eagle::Key::P
#define EG_KEY_Q               ::Eagle::Key::Q
#define EG_KEY_R               ::Eagle::Key::R
#define EG_KEY_S               ::Eagle::Key::S
#define EG_KEY_T               ::Eagle::Key::T
#define EG_KEY_U               ::Eagle::Key::U
#define EG_KEY_V               ::Eagle::Key::V
#define EG_KEY_W               ::Eagle::Key::W
#define EG_KEY_X               ::Eagle::Key::X
#define EG_KEY_Y               ::Eagle::Key::Y
#define EG_KEY_Z               ::Eagle::Key::Z
#define EG_KEY_LEFT_BRACKET    ::Eagle::Key::LeftBracket   /* [ */
#define EG_KEY_BACKSLASH       ::Eagle::Key::Backslash     /* \ */
#define EG_KEY_RIGHT_BRACKET   ::Eagle::Key::RightBracket  /* ] */
#define EG_KEY_GRAVE_ACCENT    ::Eagle::Key::GraveAccent   /* ` */
#define EG_KEY_WORLD_1         ::Eagle::Key::World1        /* non-US #1 */
#define EG_KEY_WORLD_2         ::Eagle::Key::World2        /* non-US #2 */

#define EG_KEY_ESCAPE          ::Eagle::Key::Escape
#define EG_KEY_ENTER           ::Eagle::Key::Enter
#define EG_KEY_TAB             ::Eagle::Key::Tab
#define EG_KEY_BACKSPACE       ::Eagle::Key::Backspace
#define EG_KEY_INSERT          ::Eagle::Key::Insert
#define EG_KEY_DELETE          ::Eagle::Key::Delete
#define EG_KEY_RIGHT           ::Eagle::Key::Right
#define EG_KEY_LEFT            ::Eagle::Key::Left
#define EG_KEY_DOWN            ::Eagle::Key::Down
#define EG_KEY_UP              ::Eagle::Key::Up
#define EG_KEY_PAGE_UP         ::Eagle::Key::PageUp
#define EG_KEY_PAGE_DOWN       ::Eagle::Key::PageDown
#define EG_KEY_HOME            ::Eagle::Key::Home
#define EG_KEY_END             ::Eagle::Key::End
#define EG_KEY_CAPS_LOCK       ::Eagle::Key::CapsLock
#define EG_KEY_SCROLL_LOCK     ::Eagle::Key::ScrollLock
#define EG_KEY_NUM_LOCK        ::Eagle::Key::NumLock
#define EG_KEY_PRINT_SCREEN    ::Eagle::Key::PrintScreen
#define EG_KEY_PAUSE           ::Eagle::Key::Pause
#define EG_KEY_F1              ::Eagle::Key::F1
#define EG_KEY_F2              ::Eagle::Key::F2
#define EG_KEY_F3              ::Eagle::Key::F3
#define EG_KEY_F4              ::Eagle::Key::F4
#define EG_KEY_F5              ::Eagle::Key::F5
#define EG_KEY_F6              ::Eagle::Key::F6
#define EG_KEY_F7              ::Eagle::Key::F7
#define EG_KEY_F8              ::Eagle::Key::F8
#define EG_KEY_F9              ::Eagle::Key::F9
#define EG_KEY_F10             ::Eagle::Key::F10
#define EG_KEY_F11             ::Eagle::Key::F11
#define EG_KEY_F12             ::Eagle::Key::F12
#define EG_KEY_F13             ::Eagle::Key::F13
#define EG_KEY_F14             ::Eagle::Key::F14
#define EG_KEY_F15             ::Eagle::Key::F15
#define EG_KEY_F16             ::Eagle::Key::F16
#define EG_KEY_F17             ::Eagle::Key::F17
#define EG_KEY_F18             ::Eagle::Key::F18
#define EG_KEY_F19             ::Eagle::Key::F19
#define EG_KEY_F20             ::Eagle::Key::F20
#define EG_KEY_F21             ::Eagle::Key::F21
#define EG_KEY_F22             ::Eagle::Key::F22
#define EG_KEY_F23             ::Eagle::Key::F23
#define EG_KEY_F24             ::Eagle::Key::F24
#define EG_KEY_F25             ::Eagle::Key::F25

/* Keypad */
#define EG_KEY_KP_0            ::Eagle::Key::KP0
#define EG_KEY_KP_1            ::Eagle::Key::KP1
#define EG_KEY_KP_2            ::Eagle::Key::KP2
#define EG_KEY_KP_3            ::Eagle::Key::KP3
#define EG_KEY_KP_4            ::Eagle::Key::KP4
#define EG_KEY_KP_5            ::Eagle::Key::KP5
#define EG_KEY_KP_6            ::Eagle::Key::KP6
#define EG_KEY_KP_7            ::Eagle::Key::KP7
#define EG_KEY_KP_8            ::Eagle::Key::KP8
#define EG_KEY_KP_9            ::Eagle::Key::KP9
#define EG_KEY_KP_DECIMAL      ::Eagle::Key::KPDecimal
#define EG_KEY_KP_DIVIDE       ::Eagle::Key::KPDivide
#define EG_KEY_KP_MULTIPLY     ::Eagle::Key::KPMultiply
#define EG_KEY_KP_SUBTRACT     ::Eagle::Key::KPSubtract
#define EG_KEY_KP_ADD          ::Eagle::Key::KPAdd
#define EG_KEY_KP_ENTER        ::Eagle::Key::KPEnter
#define EG_KEY_KP_EQUAL        ::Eagle::Key::KPEqual

#define EG_KEY_LEFT_SHIFT      ::Eagle::Key::LeftShift
#define EG_KEY_LEFT_CONTROL    ::Eagle::Key::LeftControl
#define EG_KEY_LEFT_ALT        ::Eagle::Key::LeftAlt
#define EG_KEY_LEFT_SUPER      ::Eagle::Key::LeftSuper
#define EG_KEY_RIGHT_SHIFT     ::Eagle::Key::RightShift
#define EG_KEY_RIGHT_CONTROL   ::Eagle::Key::RightControl
#define EG_KEY_RIGHT_ALT       ::Eagle::Key::RightAlt
#define EG_KEY_RIGHT_SUPER     ::Eagle::Key::RightSuper
#define EG_KEY_MENU            ::Eagle::Key::Menu
