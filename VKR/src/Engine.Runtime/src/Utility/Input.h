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
		*/char* GetCurrentState();

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
		int GetKey(int keyCode, EInputState state = INPUTSTATE_CURRENT);

		/*
		Description: Get a bool which is true if the key corresponding to the provided keycode was pressed this input frame.
		Return Type: int
		Param:
			int keycode: The keycode of the key to read (Most keys in ASCII).
		*/
		bool GetKeyPressed(int keyCode);

		/*
		Description: Get a bool which is true if the key corresponding to the provided keycode was released this input frame.
		Return Type: int
		Param:
			int keycode: The keycode of the key to read (Most keys in ASCII).
		*/
		bool GetKeyReleased(int keyCode);

		/*
		Description: Get the input state of the specified mouse button.
		Return Type: int
		Param:
			EMouseButton button: The button to read.
			EInputState state: The input state to read. (Can be INPUTSTATE_CURRENT or INPUTSTATE_PREVIOUS.)
		*/
		int GetMouseButton(EMouseButton button, EInputState state = INPUTSTATE_CURRENT);

		/*
		Description: Get the X coordinate of the cursor relative to the application window.
		Return Type: float
		Param:
			EInputState state: The input state to read. (Can be INPUTSTATE_CURRENT or INPUTSTATE_PREVIOUS.)
		*/
		float GetRawCursorX(EInputState state = INPUTSTATE_CURRENT);

		/*
		Description: Get the Y coordinate of the cursor relative to the application window.
		Return Type: float
		Param:
			EInputState state: The input state to read. (Can be INPUTSTATE_CURRENT or INPUTSTATE_PREVIOUS.)
		*/
		float GetRawCursorY(EInputState state = INPUTSTATE_CURRENT);

		/*
		Description: Get the X coordinate of the cursor relative to the mouse region.
		Return Type: float
		Param:
			EInputState state: The input state to read. (Can be INPUTSTATE_CURRENT or INPUTSTATE_PREVIOUS.)
		*/
		float GetCursorX(EInputState state = INPUTSTATE_CURRENT);

		/*
		Description: Get the Y coordinate of the cursor relative to the mouse region.
		Return Type: float
		Param:
			EInputState state: The input state to read. (Can be INPUTSTATE_CURRENT or INPUTSTATE_PREVIOUS.)
		*/
		float GetCursorY(EInputState state = INPUTSTATE_CURRENT);

		/*
		Description: Get the X scroll value this frame.
		Return Type: float
		Param:
			EInputState state: The input state to read. (Can be INPUTSTATE_CURRENT or INPUTSTATE_PREVIOUS.)
		*/
		float GetScrollX(EInputState state = INPUTSTATE_CURRENT);

		/*
		Description: Get the Y scroll value this frame.
		Return Type: float
		Param:
			EInputState state: The input state to read. (Can be INPUTSTATE_CURRENT or INPUTSTATE_PREVIOUS.)
		*/
		float GetScrollY(EInputState state = INPUTSTATE_CURRENT);

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