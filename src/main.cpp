#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <dwmapi.h>
#include <WinSock2.h>
#include <WS2tcpip.h>
#include <math.h>

#define COUNTOF(x) (sizeof(x)/sizeof(x[0]))

bool IsAltTabWindow(HWND hwnd)
{
	if (!IsWindowVisible(hwnd)) return false;

	// Start at the root owner
	HWND hwndWalk = GetAncestor(hwnd, GA_ROOTOWNER);

	// See if we are the last active visible popup
	HWND hwndTry;
	while ((hwndTry = GetLastActivePopup(hwndWalk)) != hwndTry) {
		if (IsWindowVisible(hwndTry)) break;
		hwndWalk = hwndTry;
	}
	return hwndWalk == hwnd;
}

const wchar_t* classes_to_avoid[] = {
	L"Shell_TrayWnd", // Task Bar
	L"DV2ControlHost", // Start Menu
	L"MsgrIMEWindowClass", // Messenger
	L"SysShadow", // Messenger
	L"Button", // UI component, e.g. Start Menu button
	L"Windows.UI.Core.CoreWindow", // Windows 10 Store Apps
	L"Frame Alternate Owner", // Edge
	L"MultitaskingViewFrame", // The original Win + Tab view
};

bool is_class_nice(const wchar_t* class_name)
{
	for (int i = 0; i < COUNTOF(classes_to_avoid); i++)
	{
		auto& class_to_avoid = classes_to_avoid[i];
		if (wcscmp(class_to_avoid, class_name) == 0)
			return false;
	}
	return true;
}

void post_message(HWND hwnd)
{
	Sleep(5000);
	PostMessageW(hwnd, WM_KEYDOWN, VK_SPACE, 0x00390001);
	//PostMessageW(hwnd, WM_CHAR, ' ', 0x00240001);
	PostMessageW(hwnd, WM_KEYUP, VK_SPACE, 0xC0390001);
}

#define DEFAULT_BUFLEN 512

const auto control_port = 4444;
const auto broadcast_port = 4445;
LARGE_INTEGER counter_frequency;

int Broadcast(SOCKET s, const char* msg, int msg_len, sockaddr* addr)
{
	auto ret = sendto(s, msg, msg_len, 0, addr, sizeof(sockaddr));
	if (ret == SOCKET_ERROR)
	{
		printf("sendto failed with error: %d\n", WSAGetLastError());
		closesocket(s);
		WSACleanup();
		return 0;
	}

    return 1;
}

struct broadcast_presence_data {
    SOCKET socket;
};

volatile auto broadcast_presence_run = true;
volatile auto mouse_x_vel = 0.0;
volatile auto mouse_y_vel = 0.0;
auto mouse_x_travel = 0.0;
auto mouse_y_travel = 0.0;
auto lock = false;

DWORD MouseUpdate(void* ptr)
{
	while (broadcast_presence_run)
	{
		mouse_x_travel += mouse_x_vel;
		mouse_y_travel += mouse_y_vel;
		auto dx = (long)mouse_x_travel;
		mouse_x_travel -= dx;
		auto dy = (long)mouse_y_travel;
		mouse_y_travel -= dy;
		INPUT input = { .type = INPUT_MOUSE, .mi = {.dx = dx, .dy = dy, .dwFlags = MOUSEEVENTF_MOVE} };
		SendInput(1, &input, sizeof(INPUT));
	}

	return 0;
}

DWORD BroadcastPresence(void* ptr)
{
    auto broadcast_socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (broadcast_socket == SOCKET_ERROR)
    {
        printf("socket failed with error: %ld\n", WSAGetLastError());
        WSACleanup();
        return 1;
    }

    char enable_broadcast = 1;
    if (setsockopt(broadcast_socket, SOL_SOCKET, SO_BROADCAST, &enable_broadcast, sizeof(enable_broadcast)) == SOCKET_ERROR)
    {
        printf("setsockopt failed with error: %ld\n", WSAGetLastError());
        closesocket(broadcast_socket);
        WSACleanup();
        return 1;
    }

    INTERFACE_INFO interface_list[20];
    unsigned long bytes_returned;
    if (WSAIoctl(broadcast_socket, SIO_GET_INTERFACE_LIST, 0, 0, &interface_list, sizeof(interface_list), &bytes_returned, 0, 0) == SOCKET_ERROR)
    {
        printf("ioctl failed with error: %ld\n", WSAGetLastError());
        closesocket(broadcast_socket);
        WSACleanup();
        return 1;
    }

	char hostname[255];
	gethostname(hostname, sizeof(hostname));

	while (broadcast_presence_run)
	{
		for (int i = 0; i < bytes_returned / sizeof(INTERFACE_INFO); i++)
		{
			u_long nFlags = interface_list[i].iiFlags;
			if (nFlags & IFF_UP && !(nFlags & IFF_LOOPBACK))
			{
				auto pAddress = (sockaddr_in*)&(interface_list[i].iiBroadcastAddress);
                char str[INET_ADDRSTRLEN];
                inet_ntop(AF_INET, &pAddress->sin_addr, str, INET_ADDRSTRLEN);

				sockaddr_in addr;
				addr.sin_family = AF_INET;
				addr.sin_port = htons(broadcast_port);
				inet_pton(AF_INET, str, &addr.sin_addr.s_addr);

				char msg_example[] = "HELLOEDIWAND|Windows|";
				//char msg_example[] = "HELLOREMOTEPOINTER|Windows|";
				char msg[COUNTOF(msg_example) + COUNTOF(hostname)];
				strcpy(msg, msg_example);
                strncat(msg, hostname, COUNTOF(hostname));

				//printf("sending msg:<%s> to %s:%i\n", msg, str, broadcast_port);

				if (!Broadcast(broadcast_socket, msg, (int)strlen(msg), (sockaddr*)&addr))
				{
					return 1;
				}
			}
		}
        Sleep(1000);
	}

	closesocket(broadcast_socket);
	WSACleanup();

    return 0;
}

LARGE_INTEGER pps_start_counter;
LARGE_INTEGER pps_show_start_counter;
float pps_show_acc = 0.f;

void handle_message(char* msg, int msg_count)
{
	if (strncmp(msg, "GYRO", 4) == 0)
	{
		LARGE_INTEGER pps_end_counter;
		QueryPerformanceCounter(&pps_end_counter);
		auto delta_ms = (pps_end_counter.QuadPart - pps_start_counter.QuadPart) * 1e3f / (float)counter_frequency.QuadPart;
		pps_start_counter = pps_end_counter;

		LARGE_INTEGER pps_show_end_counter;
		QueryPerformanceCounter(&pps_show_end_counter);
		pps_show_acc += (pps_show_end_counter.QuadPart - pps_show_start_counter.QuadPart) / (float)counter_frequency.QuadPart;
		pps_show_start_counter = pps_show_end_counter;
		if (pps_show_acc > 1)
		{
			printf("%f pps of GYRO\n", 1e3f / delta_ms);
			pps_show_acc -= (int)pps_show_acc;
		}

		auto div1 = strchr(msg, '|');
		auto x = atof(div1 + 1);
		if (*(div1 + 1) != 0)
		{
			auto div2 = strchr(div1 + 1, '|');
			auto y = atof(div2 + 1);

			mouse_x_vel = x;
			mouse_y_vel = y;
		}
	}
}

int recv_from_timeout_udp(SOCKET socket, long sec, long usec)
{
	// Setup timeval variable
	struct timeval timeout;
	struct fd_set fds;
	timeout.tv_sec = sec;
	timeout.tv_usec = usec;

	// Setup fd_set structure
	FD_ZERO(&fds);
	FD_SET(socket, &fds);

	// Return value:
	// -1: error occurred
	// 0: timed out
	// > 0: data ready to be read
	return select(0, &fds, 0, 0, &timeout);
}

int main()
{
	QueryPerformanceFrequency(&counter_frequency);

    // Initialize Winsock
    WSADATA wsaData;
    auto iResult = WSAStartup(MAKEWORD(2,2), &wsaData);
    if (iResult != 0) {
        printf("WSAStartup failed with error: %d\n", iResult);
        return 1;
    }

	/*
    auto broadcast_thread_memory = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(broadcast_presence_data));
    ((broadcast_presence_data*)broadcast_thread_memory)->socket = broadcast_socket;
	*/

    auto broadcast_thread_handle = CreateThread(0, 0, BroadcastPresence, 0, 0, 0);
    auto mouse_thread_handle = CreateThread(0, 0, MouseUpdate, 0, 0, 0);

#if 0
    auto control_socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (control_socket == SOCKET_ERROR)
    {
        printf("socket failed with error: %ld\n", WSAGetLastError());
        WSACleanup();
        return 1;
    }

	SOCKADDR_IN recv_addr = {};
	recv_addr.sin_family = AF_INET;
	recv_addr.sin_port = htons(control_port);
	recv_addr.sin_addr.s_addr = htonl(INADDR_ANY);

	// Setup the UDP listening socket
	iResult = bind(control_socket, (SOCKADDR*)&recv_addr, sizeof(recv_addr));
	if (iResult == SOCKET_ERROR) {
		printf("bind failed with error: %d\n", WSAGetLastError());
		closesocket(control_socket);
		WSACleanup();
		return 1;
	}

	int namelen = sizeof(recv_addr);
	iResult = getsockname(control_socket, (SOCKADDR*)&recv_addr, &namelen);
	if (iResult == SOCKET_ERROR) {
		printf("getsockname failed with error: %d\n", WSAGetLastError());
		closesocket(control_socket);
		WSACleanup();
		return 1;
	}

	char str[INET_ADDRSTRLEN];
	inet_ntop(AF_INET, &recv_addr.sin_addr, str, INET_ADDRSTRLEN);
	printf("Server: Receiving IP(s) used : %s\n", str);
	printf("Server: Receiving port used : %d\n", htons(recv_addr.sin_port));
	printf("Server: I\'m ready to receive a datagram...\n");


	iResult = recv_from_timeout_udp(control_socket, 10, 0);
	if (iResult == SOCKET_ERROR) {
		printf("select failed with error: %d\n", WSAGetLastError());
		closesocket(control_socket);
		WSACleanup();
		return 1;
	}

	switch (iResult)
	{
	case 0:
		// Timed out, do whatever you want to handle this situation
		printf("Server: Timeout lor while waiting you bastard client!...\n");
		break;
	case -1:
		// Error occurred, maybe we should display an error message?
		// Need more tweaking here and the recvfromTimeOutUDP()...
		printf("Server: Some error encountered with code number: %ld\n", WSAGetLastError());
		break;
	default:
		while (1)
		{
			// Call recvfrom() to get it then display the received data...
			SOCKADDR_IN sender_addr;
			char recv_buf[1024];
			auto bytes_received = recvfrom(control_socket, recv_buf, COUNTOF(recv_buf), 0, (SOCKADDR*)&sender_addr, (int*)sizeof(sender_addr));
			if (bytes_received > 0)
			{
				printf("Server: Total Bytes received: %d\n", bytes_received);
				printf("Server: The data is %s\n", recv_buf);
			}
			else if (bytes_received <= 0)
				printf("Server: Connection closed with error code: %ld\n", WSAGetLastError());
			else
				printf("Server: recvfrom() failed with error code: %ld\n", WSAGetLastError());

			// Some info on the sender side
			getpeername(control_socket, (SOCKADDR*)&sender_addr, (int*)sizeof(sender_addr));
			inet_ntop(AF_INET, &sender_addr.sin_addr, str, INET_ADDRSTRLEN);
			printf("Server: Sending IP used: %s\n", str);
			printf("Server: Sending port used: %d\n", htons(sender_addr.sin_port));
		}
	}

	closesocket(control_socket);
	WSACleanup();

#else
    struct addrinfo hints;
    ZeroMemory(&hints, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;
    hints.ai_flags = AI_PASSIVE;

    struct addrinfo *result = NULL;

    // Resolve the server address and port
    char control_port_str[5];
    snprintf(control_port_str, COUNTOF(control_port_str), "%d", control_port);
    iResult = getaddrinfo(NULL, control_port_str, &hints, &result);
    if ( iResult != 0 ) {
        printf("getaddrinfo failed with error: %d\n", iResult);
        WSACleanup();
        return 1;
    }

	// Create a SOCKET for connecting to server
	auto ListenSocket = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
	if (ListenSocket == INVALID_SOCKET) {
		printf("socket failed with error: %ld\n", WSAGetLastError());
		freeaddrinfo(result);
		WSACleanup();
		return 1;
	}

	// Setup the TCP listening socket
	iResult = bind(ListenSocket, result->ai_addr, (int)result->ai_addrlen);
	if (iResult == SOCKET_ERROR) {
		printf("bind failed with error: %d\n", WSAGetLastError());
		freeaddrinfo(result);
		closesocket(ListenSocket);
		WSACleanup();
		return 1;
	}

	freeaddrinfo(result);

	iResult = listen(ListenSocket, SOMAXCONN);
	if (iResult == SOCKET_ERROR) {
		printf("listen failed with error: %d\n", WSAGetLastError());
		closesocket(ListenSocket);
		WSACleanup();
		return 1;
	}

    while (true)
    {
		// Accept a client socket
		auto ClientSocket = accept(ListenSocket, NULL, NULL);
		if (ClientSocket == INVALID_SOCKET) {
			printf("accept failed with error: %d\n", WSAGetLastError());
			closesocket(ListenSocket);
			WSACleanup();
			return 1;
		}

		char recvbuf[DEFAULT_BUFLEN];
		int recvbuflen = DEFAULT_BUFLEN;

		// Echo the buffer back to the sender
		char msg[] = "BOTAQUETEM\n";
		auto iSendResult = send(ClientSocket, msg, COUNTOF(msg), 0);
		if (iSendResult == SOCKET_ERROR) {
			printf("send failed with error: %d\n", WSAGetLastError());
			closesocket(ClientSocket);
			WSACleanup();
			return 1;
		}
		printf("Bytes sent: %d\n", iSendResult);
		QueryPerformanceCounter(&pps_show_start_counter);
		QueryPerformanceCounter(&pps_start_counter);

		// Receive until the peer shuts down the connection
		do {
			iResult = recv(ClientSocket, recvbuf, recvbuflen, 0);
			if (iResult > 0) {
				//printf("Bytes received: %d\n", iResult);
				//printf("%s\n", recvbuf);

				handle_message(recvbuf, iResult);

			}
			else if (iResult == 0)
				printf("Connection closing...\n");
			else {
				printf("recv failed with error: %d\n", WSAGetLastError());
				closesocket(ClientSocket);
				WSACleanup();
				return 1;
			}

		} while (iResult > 0);

		// shutdown the connection since we're done
		iResult = shutdown(ClientSocket, SD_SEND);
		if (iResult == SOCKET_ERROR) {
			printf("shutdown failed with error: %d\n", WSAGetLastError());
			closesocket(ClientSocket);
			WSACleanup();
			return 1;
		}

		// cleanup
		closesocket(ClientSocket);
	}

	// No longer need server socket
	closesocket(ListenSocket);

	WSACleanup();
#endif

    broadcast_presence_run = false;
    WaitForSingleObject(broadcast_thread_handle, INFINITE);
    CloseHandle(broadcast_thread_handle);
	/*
    HeapFree(GetProcessHeap(), 0, broadcast_thread_memory);
	*/

    WaitForSingleObject(mouse_thread_handle, INFINITE);
    CloseHandle(mouse_thread_handle);

}
			/*
			printf("\n");

			sockaddr_in *pAddress;
			pAddress = (sockaddr_in *) & (interface_list[i].iiAddress);
			printf(" %s", inet_ntoa(pAddress->sin_addr));

			pAddress = (sockaddr_in *) & (interface_list[i].iiBroadcastAddress);
			printf(" has bcast %s", inet_ntoa(pAddress->sin_addr));

			pAddress = (sockaddr_in *) & (interface_list[i].iiNetmask);
			printf(" and netmask %s\n", inet_ntoa(pAddress->sin_addr));

			printf(" Iface is ");
			u_long nFlags = interface_list[i].iiFlags;
			if (nFlags & IFF_UP) printf("up");
			else                 printf("down");
			if (nFlags & IFF_POINTTOPOINT) printf(", is point-to-point");
			if (nFlags & IFF_LOOPBACK)     printf(", is a loopback iface");
			printf(", and can do: ");
			if (nFlags & IFF_BROADCAST) printf("bcast ");
			if (nFlags & IFF_MULTICAST) printf("multicast ");
			printf("\n");
			*/

#if 0
int main()
{
    WSADATA wsaData;
    int iResult;

    SOCKET ListenSocket = INVALID_SOCKET;
    SOCKET ClientSocket = INVALID_SOCKET;

    struct addrinfo *result = NULL;
    struct addrinfo hints;

    int iSendResult;
    char recvbuf[DEFAULT_BUFLEN];
    int recvbuflen = DEFAULT_BUFLEN;
    
    // Initialize Winsock
    iResult = WSAStartup(MAKEWORD(2,2), &wsaData);
    if (iResult != 0) {
        printf("WSAStartup failed with error: %d\n", iResult);
        return 1;
    }

    ZeroMemory(&hints, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;
    hints.ai_flags = AI_PASSIVE;

    // Resolve the server address and port
    iResult = getaddrinfo(NULL, DEFAULT_PORT, &hints, &result);
    if ( iResult != 0 ) {
        printf("getaddrinfo failed with error: %d\n", iResult);
        WSACleanup();
        return 1;
    }

    // Create a SOCKET for connecting to server
    ListenSocket = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
    if (ListenSocket == INVALID_SOCKET) {
        printf("socket failed with error: %ld\n", WSAGetLastError());
        freeaddrinfo(result);
        WSACleanup();
        return 1;
    }

    // Setup the TCP listening socket
    iResult = bind( ListenSocket, result->ai_addr, (int)result->ai_addrlen);
    if (iResult == SOCKET_ERROR) {
        printf("bind failed with error: %d\n", WSAGetLastError());
        freeaddrinfo(result);
        closesocket(ListenSocket);
        WSACleanup();
        return 1;
    }

    freeaddrinfo(result);

    iResult = listen(ListenSocket, SOMAXCONN);
    if (iResult == SOCKET_ERROR) {
        printf("listen failed with error: %d\n", WSAGetLastError());
        closesocket(ListenSocket);
        WSACleanup();
        return 1;
    }

    // Accept a client socket
    ClientSocket = accept(ListenSocket, NULL, NULL);
    if (ClientSocket == INVALID_SOCKET) {
        printf("accept failed with error: %d\n", WSAGetLastError());
        closesocket(ListenSocket);
        WSACleanup();
        return 1;
    }

    // No longer need server socket
    closesocket(ListenSocket);

    // Receive until the peer shuts down the connection
    do {

        iResult = recv(ClientSocket, recvbuf, recvbuflen, 0);
        if (iResult > 0) {
            printf("Bytes received: %d\n", iResult);

        // Echo the buffer back to the sender
            iSendResult = send( ClientSocket, recvbuf, iResult, 0 );
            if (iSendResult == SOCKET_ERROR) {
                printf("send failed with error: %d\n", WSAGetLastError());
                closesocket(ClientSocket);
                WSACleanup();
                return 1;
            }
            printf("Bytes sent: %d\n", iSendResult);
        }
        else if (iResult == 0)
            printf("Connection closing...\n");
        else  {
            printf("recv failed with error: %d\n", WSAGetLastError());
            closesocket(ClientSocket);
            WSACleanup();
            return 1;
        }

    } while (iResult > 0);

    // shutdown the connection since we're done
    iResult = shutdown(ClientSocket, SD_SEND);
    if (iResult == SOCKET_ERROR) {
        printf("shutdown failed with error: %d\n", WSAGetLastError());
        closesocket(ClientSocket);
        WSACleanup();
        return 1;
    }

    // cleanup
    closesocket(ClientSocket);
    WSACleanup();

    return 0;

#if 0
	DWORD cntrl_thread = 0;
	HWND cntrl_window = 0;
	for (auto hwnd = GetTopWindow(0); hwnd; hwnd = GetNextWindow(hwnd, GW_HWNDNEXT))
	{
		int length = GetWindowTextLengthW(hwnd);
		if (length != 0 && IsAltTabWindow(hwnd))
		{
			WCHAR class_name[256];
			GetClassNameW(hwnd, class_name, COUNTOF(class_name));
			auto window_title = new WCHAR[length + 1];
			GetWindowTextW(hwnd, window_title, length + 1);
			if (wcscmp(class_name, L"ApplicationFrameWindow") == 0)
			{
				// UWP app
				int cloaked;
				DwmGetWindowAttribute(hwnd, DWMWA_CLOAKED, &cloaked, sizeof(cloaked));
				if (!cloaked)
					printf("%ls\n", window_title);
			}
			else if (is_class_nice(class_name))
			{
				if (wcsstr(window_title, L"Notepad"))
				{
					DWORD pid;
					cntrl_window = hwnd;
					cntrl_thread = GetWindowThreadProcessId(hwnd, &pid);
					printf("GOtiit\n");
				}
				if (wcscmp(window_title, L"Program Manager") != 0)
					printf("%ls\n", window_title);
			}
		}
	}

	SetForegroundWindow(cntrl_window);
	INPUT input = { .type = INPUT_KEYBOARD, .ki = {.wVk = VK_SPACE} };
	SendInput(1, &input, sizeof(INPUT));
	input = { .type = INPUT_MOUSE, .mi = {.dx = 5, .dy = 5, .dwFlags = MOUSEEVENTF_MOVE} };
	SendInput(1, &input, sizeof(INPUT));
#endif

	/*
	//printf("-----------\n");
	post_message(cntrl_window);

	for (auto hwnd = GetTopWindow(cntrl_window); hwnd; hwnd = GetNextWindow(hwnd, GW_HWNDNEXT))
	{
		int length = GetWindowTextLengthW(hwnd);
		if (IsWindowVisible(hwnd))
		{
			auto window_title = new WCHAR[length + 1];
			GetWindowTextW(hwnd, window_title, length + 1);
			//printf("%p, %ls\n", hwnd, window_title);

			post_message(hwnd);
		}
	}
	*/
}
#endif