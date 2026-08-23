#pragma once

namespace std
{
     [[vk::binding(0,1)]] cbuffer k
     {
        float4 keys[128];
     };

     buffer mouse
     {
        float2 mouse_pos;//current mouse position in pixels
        float2 mouse_delta;//mouse delta for the last frame in pixels
        float mouse_wheel_delta;//mouse wheel delta for the last frame
        bool lmb_down;//if left mouse button down
        bool rmb_down;//if right mouse button down
        bool mmb_down;//if middle mouse button down
     }


    enum KeyCode {
        None = 0,
        Tab = 1,
        LeftArrow = 2,
        RightArrow = 3,
        UpArrow = 4,
        DownArrow = 5,
        PageUp = 6,
        PageDown = 7,
        Home = 8,
        End = 9,
        Insert = 10,
        Delete = 11,
        Backspace = 12,
        Space = 13,
        Enter = 14,
        Escape = 15,
        LeftCtrl = 16,
        LeftShift = 17,
        LeftAlt = 18,
        LeftSuper = 19,
        RightCtrl = 20,
        RightShift = 21,
        RightAlt = 22,
        RightSuper = 23,
        Menu = 24,
        _0 = 25,
        _1 = 26,
        _2 = 27,
        _3 = 28,
        _4 = 29,
        _5 = 30,
        _6 = 31,
        _7 = 32,
        _8 = 33,
        _9 = 34,
        A = 35,
        B = 36,
        C = 37,
        D = 38,
        E = 39,
        F = 40,
        G = 41,
        H = 42,
        I = 43,
        J = 44,
        K = 45,
        L = 46,
        M = 47,
        N = 48,
        O = 49,
        P = 50,
        Q = 51,
        R = 52,
        S = 53,
        T = 54,
        U = 55,
        V = 56,
        W = 57,
        X = 58,
        Y = 59,
        Z = 60,
        F1 = 61,
        F2 = 62,
        F3 = 63,
        F4 = 64,
        F5 = 65,
        F6 = 66,
        F7 = 67,
        F8 = 68,
        F9 = 69,
        F10 = 70,
        F11 = 71,
        F12 = 72,
        Apostrophe = 73,
        Comma = 74,
        Minus = 75,
        Period = 76,
        Slash = 77,
        Semicolon = 78,
        Equal = 79,
        LeftBracket = 80,
        Backslash = 81,
        RightBracket = 82,
        GraveAccent = 83,
        CapsLock = 84,
        ScrollLock = 85,
        NumLock = 86,
        PrintScreen = 87,
        Pause = 88,
        Keypad0 = 89,
        Keypad1 = 90,
        Keypad2 = 91,
        Keypad3 = 92,
        Keypad4 = 93,
        Keypad5 = 94,
        Keypad6 = 95,
        Keypad7 = 96,
        Keypad8 = 97,
        Keypad9 = 98,
        KeypadDecimal = 99,
        KeypadDivide = 100,
        KeypadMultiply = 101,
        KeypadSubtract = 102,
        KeypadAdd = 103,
        KeypadEnter = 104,
        KeypadEqual = 105,
        GamepadStart = 106,
        GamepadBack = 107,
        GamepadFaceLeft = 108,
        GamepadFaceRight = 109,
        GamepadFaceUp = 110,
        GamepadFaceDown = 111,
        GamepadDpadLeft = 112,
        GamepadDpadRight = 113,
        GamepadDpadUp = 114,
        GamepadDpadDown = 115,
        GamepadL1 = 116,
        GamepadR1 = 117,
        GamepadL2 = 118,
        GamepadR2 = 119,
        GamepadL3 = 120,
        GamepadR3 = 121,
        GamepadLStickLeft = 122,
        GamepadLStickRight = 123,
        GamepadLStickUp = 124,
        GamepadLStickDown = 125,
        GamepadRStickLeft = 126,
        GamepadRStickRight = 127,
        GamepadRStickUp = 128,
        GamepadRStickDown = 129,
        MouseLeft = 130,
        MouseRight = 131,
        MouseMiddle = 132,
        MouseX1 = 133,
        MouseX2 = 134,
        MouseWheelX = 135,
        MouseWheelY = 136,
        Ctrl = 137,
        Shift = 138,
        Alt = 139,
        Super = 140
    };


     bool isKeyPressed(int key)
     {
       return keys[key/4][key%4]>1;
     }

     bool isKeyReleased(int key)
     {
       return keys[key/4][key%4]<0;
     }

     bool isKeyDown(int key)
     {
       return keys[key/4][key%4]>0;
     }
 }