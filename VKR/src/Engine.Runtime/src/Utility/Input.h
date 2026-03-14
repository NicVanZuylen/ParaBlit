#pragma once

#include "Engine.Math/Vectors.h"

namespace Eng
{
	enum EInputState
	{
		INPUTSTATE_CURRENT,
		INPUTSTATE_PREVIOUS
	};

	enum EMouseButton
	{
		MOUSEBUTTON_LEFT,
		MOUSEBUTTON_RIGHT,
		MOUSEBUTTON_MIDDLE,
		MOUSEBUTTON_3,
		MOUSEBUTTON_4,
		MOUSEBUTTON_5,
		MOUSEBUTTON_6,
		MOUSEBUTTON_7
	};

	enum EKeyboardKey : int
	{
		KEYBOARD_KEY_UNKNOWN = -1,

		KEYBOARD_KEY_SPACE            = 32,
		KEYBOARD_KEY_APOSTROPHE       = 39,  /* ' */
		KEYBOARD_KEY_COMMA            = 44,  /* , */
		KEYBOARD_KEY_MINUS            = 45,  /* - */
		KEYBOARD_KEY_PERIOD           = 46,  /* . */
		KEYBOARD_KEY_SLASH            = 47,  /* / */
		KEYBOARD_KEY_0                = 48,
		KEYBOARD_KEY_1                = 49,
		KEYBOARD_KEY_2                = 50,
		KEYBOARD_KEY_3                = 51,
		KEYBOARD_KEY_4                = 52,
		KEYBOARD_KEY_5                = 53,
		KEYBOARD_KEY_6                = 54,
		KEYBOARD_KEY_7                = 55,
		KEYBOARD_KEY_8                = 56,
		KEYBOARD_KEY_9                = 57,
		KEYBOARD_KEY_SEMICOLON        = 59,  /* ; */
		KEYBOARD_KEY_EQUAL            = 61,  /* = */
		KEYBOARD_KEY_A                = 65,
		KEYBOARD_KEY_B                = 66,
		KEYBOARD_KEY_C                = 67,
		KEYBOARD_KEY_D                = 68,
		KEYBOARD_KEY_E                = 69,
		KEYBOARD_KEY_F                = 70,
		KEYBOARD_KEY_G                = 71,
		KEYBOARD_KEY_H                = 72,
		KEYBOARD_KEY_I                = 73,
		KEYBOARD_KEY_J                = 74,
		KEYBOARD_KEY_K                = 75,
		KEYBOARD_KEY_L                = 76,
		KEYBOARD_KEY_M                = 77,
		KEYBOARD_KEY_N                = 78,
		KEYBOARD_KEY_O                = 79,
		KEYBOARD_KEY_P                = 80,
		KEYBOARD_KEY_Q                = 81,
		KEYBOARD_KEY_R                = 82,
		KEYBOARD_KEY_S                = 83,
		KEYBOARD_KEY_T                = 84,
		KEYBOARD_KEY_U                = 85,
		KEYBOARD_KEY_V                = 86,
		KEYBOARD_KEY_W                = 87,
		KEYBOARD_KEY_X                = 88,
		KEYBOARD_KEY_Y                = 89,
		KEYBOARD_KEY_Z                = 90,
		KEYBOARD_KEY_LEFT_BRACKET     = 91,  /* [ */
		KEYBOARD_KEY_BACKSLASH        = 92,  /* \ */
		KEYBOARD_KEY_RIGHT_BRACKET    = 93,  /* ] */
		KEYBOARD_KEY_GRAVE_ACCENT     = 96,  /* ` */
		KEYBOARD_KEY_WORLD_1          = 161, /* non-US #1 */
		KEYBOARD_KEY_WORLD_2          = 162, /* non-US #2 */

		KEYBOARD_KEY_ESCAPE           = 256,
		KEYBOARD_KEY_ENTER            = 257,
		KEYBOARD_KEY_TAB              = 258,
		KEYBOARD_KEY_BACKSPACE        = 259,
		KEYBOARD_KEY_INSERT           = 260,
		KEYBOARD_KEY_DELETE           = 261,
		KEYBOARD_KEY_RIGHT            = 262,
		KEYBOARD_KEY_LEFT             = 263,
		KEYBOARD_KEY_DOWN             = 264,
		KEYBOARD_KEY_UP               = 265,
		KEYBOARD_KEY_PAGE_UP          = 266,
		KEYBOARD_KEY_PAGE_DOWN        = 267,
		KEYBOARD_KEY_HOME             = 268,
		KEYBOARD_KEY_END              = 269,
		KEYBOARD_KEY_CAPS_LOCK        = 280,
		KEYBOARD_KEY_SCROLL_LOCK      = 281,
		KEYBOARD_KEY_NUM_LOCK         = 282,
		KEYBOARD_KEY_PRINT_SCREEN     = 283,
		KEYBOARD_KEY_PAUSE            = 284,
		KEYBOARD_KEY_F1               = 290,
		KEYBOARD_KEY_F2               = 291,
		KEYBOARD_KEY_F3               = 292,
		KEYBOARD_KEY_F4               = 293,
		KEYBOARD_KEY_F5               = 294,
		KEYBOARD_KEY_F6               = 295,
		KEYBOARD_KEY_F7               = 296,
		KEYBOARD_KEY_F8               = 297,
		KEYBOARD_KEY_F9               = 298,
		KEYBOARD_KEY_F10              = 299,
		KEYBOARD_KEY_F11              = 300,
		KEYBOARD_KEY_F12              = 301,
		KEYBOARD_KEY_F13              = 302,
		KEYBOARD_KEY_F14              = 303,
		KEYBOARD_KEY_F15              = 304,
		KEYBOARD_KEY_F16              = 305,
		KEYBOARD_KEY_F17              = 306,
		KEYBOARD_KEY_F18              = 307,
		KEYBOARD_KEY_F19              = 308,
		KEYBOARD_KEY_F20              = 309,
		KEYBOARD_KEY_F21              = 310,
		KEYBOARD_KEY_F22              = 311,
		KEYBOARD_KEY_F23              = 312,
		KEYBOARD_KEY_F24              = 313,
		KEYBOARD_KEY_F25              = 314,
		KEYBOARD_KEY_KP_0             = 320,
		KEYBOARD_KEY_KP_1             = 321,
		KEYBOARD_KEY_KP_2             = 322,
		KEYBOARD_KEY_KP_3             = 323,
		KEYBOARD_KEY_KP_4             = 324,
		KEYBOARD_KEY_KP_5             = 325,
		KEYBOARD_KEY_KP_6             = 326,
		KEYBOARD_KEY_KP_7             = 327,
		KEYBOARD_KEY_KP_8             = 328,
		KEYBOARD_KEY_KP_9             = 329,
		KEYBOARD_KEY_KP_DECIMAL       = 330,
		KEYBOARD_KEY_KP_DIVIDE        = 331,
		KEYBOARD_KEY_KP_MULTIPLY      = 332,
		KEYBOARD_KEY_KP_SUBTRACT      = 333,
		KEYBOARD_KEY_KP_ADD           = 334,
		KEYBOARD_KEY_KP_ENTER         = 335,
		KEYBOARD_KEY_KP_EQUAL         = 336,
		KEYBOARD_KEY_LEFT_SHIFT       = 340,
		KEYBOARD_KEY_LEFT_CONTROL     = 341,
		KEYBOARD_KEY_LEFT_ALT         = 342,
		KEYBOARD_KEY_LEFT_SUPER       = 343,
		KEYBOARD_KEY_RIGHT_SHIFT      = 344,
		KEYBOARD_KEY_RIGHT_CONTROL    = 345,
		KEYBOARD_KEY_RIGHT_ALT        = 346,
		KEYBOARD_KEY_RIGHT_SUPER      = 347,
		KEYBOARD_KEY_MENU             = 348,
	};

	struct MouseState
	{
		char m_buttons[8];
		double m_fMouseAxes[4];
	};

	class Input
	{
	public:

		Input();

		~Input();

		void SetMouseRegion(Math::Vector2f origin, Math::Vector2f extent);

		/*
		Description: Get the raw keyboard state data this frame.
		Return Type: char
		*/
		char* GetCurrentState();

		/*
		Description: Get the raw mouse/cursor state data this frame.
		Return Type: MouseState*
		*/
		MouseState* GetCurrentMouseState();

		/*
		Description: Get the input state of the specified key.
		Return Type: int
		Param:
			int keycode: The keycode of the key to read (Most keys in ASCII).
			EInputState state: The input state to read. (Can be INPUTSTATE_CURRENT or INPUTSTATE_PREVIOUS.)
		*/
		int GetKey(EKeyboardKey keyCode, EInputState state = INPUTSTATE_CURRENT) const;

		/*
		Description: Get a bool which is true if the key corresponding to the provided keycode was pressed this input frame.
		Return Type: int
		Param:
			int keycode: The keycode of the key to read (Most keys in ASCII).
		*/
		bool GetKeyPressed(EKeyboardKey keyCode) const;

		/*
		Description: Get a bool which is true if the key corresponding to the provided keycode was released this input frame.
		Return Type: int
		Param:
			int keycode: The keycode of the key to read (Most keys in ASCII).
		*/
		bool GetKeyReleased(EKeyboardKey keyCode) const;

		/*
		Description: Get the input state of the specified mouse button.
		Return Type: int
		Param:
			EMouseButton button: The button to read.
			EInputState state: The input state to read. (Can be INPUTSTATE_CURRENT or INPUTSTATE_PREVIOUS.)
		*/
		int GetMouseButton(EMouseButton button, EInputState state = INPUTSTATE_CURRENT) const;

		/*
		Description: Get the X coordinate of the cursor relative to the application window.
		Return Type: float
		Param:
			EInputState state: The input state to read. (Can be INPUTSTATE_CURRENT or INPUTSTATE_PREVIOUS.)
		*/
		float GetRawCursorX(EInputState state = INPUTSTATE_CURRENT) const;

		/*
		Description: Get the Y coordinate of the cursor relative to the application window.
		Return Type: float
		Param:
			EInputState state: The input state to read. (Can be INPUTSTATE_CURRENT or INPUTSTATE_PREVIOUS.)
		*/
		float GetRawCursorY(EInputState state = INPUTSTATE_CURRENT) const;

		/*
		Description: Get the X coordinate of the cursor relative to the mouse region.
		Return Type: float
		Param:
			EInputState state: The input state to read. (Can be INPUTSTATE_CURRENT or INPUTSTATE_PREVIOUS.)
		*/
		float GetCursorX(EInputState state = INPUTSTATE_CURRENT) const;

		/*
		Description: Get the Y coordinate of the cursor relative to the mouse region.
		Return Type: float
		Param:
			EInputState state: The input state to read. (Can be INPUTSTATE_CURRENT or INPUTSTATE_PREVIOUS.)
		*/
		float GetCursorY(EInputState state = INPUTSTATE_CURRENT) const;

		/*
		Description: Get the X scroll value this frame.
		Return Type: float
		Param:
			EInputState state: The input state to read. (Can be INPUTSTATE_CURRENT or INPUTSTATE_PREVIOUS.)
		*/
		float GetScrollX(EInputState state = INPUTSTATE_CURRENT) const;

		/*
		Description: Get the Y scroll value this frame.
		Return Type: float
		Param:
			EInputState state: The input state to read. (Can be INPUTSTATE_CURRENT or INPUTSTATE_PREVIOUS.)
		*/
		float GetScrollY(EInputState state = INPUTSTATE_CURRENT) const;

		/*
		Description: Swaps the current keyboard state to the previous keyboard state.
		*/
		void EndFrame();

		/*
		Description: Resets all input states.
		*/
		void ResetStates();

		// Singleton functions.

		static void Create();
		static void Destroy();
		static Input* GetInstance();

	private:

		static Input* m_instance;

		char* m_keyStates[2];
		MouseState* m_mouseStates[2];

		char* m_currentState;
		char* m_prevState;

		MouseState* m_currentMouseState;
		MouseState* m_prevMouseState;
		Math::Vector2f m_mouseOrigin{};
		Math::Vector2f m_mouseExtent{};
	};
};