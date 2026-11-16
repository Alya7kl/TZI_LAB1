#ifndef PIPE_CLIENT_INCLUDE
#define PIPE_CLIENT_INCLUDE

#include <windows.h>

template <class T>
class CPipeClient
{
private:
    HANDLE hPipe;

public:
    CPipeClient()
    {
        hPipe = INVALID_HANDLE_VALUE;
    }

    ~CPipeClient()
    {
        if (hPipe != INVALID_HANDLE_VALUE)
        {
            CloseHandle(hPipe);
        }
    }

    bool ConnectPipe(char* PipeName, bool WaitInfinite = false)
    {
        if (hPipe != INVALID_HANDLE_VALUE)
        {
            CloseHandle(hPipe);
            hPipe = INVALID_HANDLE_VALUE;
        }

        while (true)
        {
            hPipe = CreateFileA(
                PipeName,
                GENERIC_READ | GENERIC_WRITE,
                0,
                NULL,
                OPEN_EXISTING,
                0,
                NULL
            );

            if (hPipe != INVALID_HANDLE_VALUE)
            {
                return true;
            }

            DWORD errorCode = GetLastError();
            if (errorCode == ERROR_PIPE_BUSY)
            {
                DWORD waitMode = WaitInfinite ? NMPWAIT_WAIT_FOREVER : NMPWAIT_USE_DEFAULT_WAIT;
                if (!WaitNamedPipeA(PipeName, waitMode))
                {
                    return false;
                }
            }
            else
            {
                return false;
            }
        }
    }

    bool WriteMessage(const T& Message)
    {
        if (hPipe == INVALID_HANDLE_VALUE)
        {
            return false;
        }

        DWORD bytesWritten = 0;
        BOOL result = WriteFile(hPipe, &Message, sizeof(T), &bytesWritten, NULL);
        return result == TRUE && bytesWritten == sizeof(T);
    }

    bool ReadResponse(char& response)
    {
        if (hPipe == INVALID_HANDLE_VALUE)
        {
            return false;
        }

        DWORD bytesRead = 0;
        BOOL result = ReadFile(hPipe, &response, sizeof(response), &bytesRead, NULL);
        return result == TRUE && bytesRead == sizeof(response);
    }

    bool IsPipeConnected()
    {
        return hPipe != INVALID_HANDLE_VALUE;
    }
};

#endif