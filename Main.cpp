#include <iostream>
#include <windows.h>
#include <vector>

using namespace std;

int* arr = nullptr;
int arr_size = 0;
int marker_count = 0;

CRITICAL_SECTION cs;

vector<HANDLE> hStartEvents;
vector<HANDLE> hContinueEvents;
vector<HANDLE> hKillEvents;
vector<HANDLE> hDeadEvents;

vector<bool> marker_is_dead;
vector<bool> marker_finished;

DWORD WINAPI marker_thread(LPVOID lpParam) {
    int thread_num = *static_cast<int*>(lpParam);
    int index_in_arr = thread_num - 1;

    srand(thread_num);

    WaitForSingleObject(hStartEvents[index_in_arr], INFINITE);

    while (true) {
        int rand_val = rand();
        int index = rand_val % arr_size;

        EnterCriticalSection(&cs);

        if (arr[index] == 0) {
            Sleep(5);
            arr[index] = thread_num;
            Sleep(5);

            LeaveCriticalSection(&cs);
            continue;
        }
        else {
            int marked_count = 0;
            for (int i = 0; i < arr_size; i++) {
                if (arr[i] == thread_num) {
                    marked_count++;
                }
            }

            cout << "\n[Поток marker " << thread_num << "]" << endl;
            cout << "  - Порядковый номер: " << thread_num << endl;
            cout << "  - Количество помеченных элементов: " << marked_count << endl;
            cout << "  - Индекс элемента массива, который невозможно пометить: " << index << endl;

            marker_is_dead[index_in_arr] = true;

            LeaveCriticalSection(&cs);

            SetEvent(hDeadEvents[index_in_arr]);

            HANDLE wait_handles[2] = { hContinueEvents[index_in_arr], hKillEvents[index_in_arr] };
            DWORD wait_result = WaitForMultipleObjects(2, wait_handles, FALSE, INFINITE);

            if (wait_result == WAIT_OBJECT_0 + 1) {
                EnterCriticalSection(&cs);
                for (int i = 0; i < arr_size; i++) {
                    if (arr[i] == thread_num) {
                        arr[i] = 0;
                    }
                }
                marker_finished[index_in_arr] = true;
                LeaveCriticalSection(&cs);

                return 0;
            }

            if (wait_result == WAIT_OBJECT_0) {
                EnterCriticalSection(&cs);
                marker_is_dead[index_in_arr] = false;
                LeaveCriticalSection(&cs);
                continue;
            }
        }
    }
    return 0;
}

int main() {
    system("chcp 1251 > nul");
    setlocale(LC_ALL, "Russian");

    InitializeCriticalSection(&cs);

    cout << "Введите размерность массива: ";
    cin >> arr_size;

    if (arr_size <= 0) {
        cerr << "Размер массива должен быть больше нуля!" << endl;
        DeleteCriticalSection(&cs);
        return 1;
    }

    arr = new int[arr_size];

    for (int i = 0; i < arr_size; i++) {
        arr[i] = 0;
    }

    cout << "Введите количество потоков marker: ";
    cin >> marker_count;

    if (marker_count <= 0) {
        cerr << "Количество потоков должно быть больше нуля!" << endl;
        delete[] arr;
        DeleteCriticalSection(&cs);
        return 1;
    }

    vector<HANDLE> hThreads(marker_count);
    vector<int> thread_IDs(marker_count);

    hStartEvents.resize(marker_count);
    hContinueEvents.resize(marker_count);
    hKillEvents.resize(marker_count);
    hDeadEvents.resize(marker_count);

    marker_is_dead.resize(marker_count, false);
    marker_finished.resize(marker_count, false);

    for (int i = 0; i < marker_count; i++) {
        hStartEvents[i] = CreateEvent(NULL, TRUE, FALSE, NULL);
        hContinueEvents[i] = CreateEvent(NULL, FALSE, FALSE, NULL);
        hKillEvents[i] = CreateEvent(NULL, FALSE, FALSE, NULL);
        hDeadEvents[i] = CreateEvent(NULL, FALSE, FALSE, NULL);
    }

    for (int i = 0; i < marker_count; i++) {
        thread_IDs[i] = i + 1;
        hThreads[i] = CreateThread(NULL, 0, marker_thread, &thread_IDs[i], 0, NULL);
        if (hThreads[i] == NULL) {
            cerr << "Ошибка создания потока " << i + 1 << endl;
        }
    }

    cout << "\n[main] Даем сигнал на старт всем потокам marker...\n" << endl;
    for (int i = 0; i < marker_count; i++) {
        SetEvent(hStartEvents[i]);
    }

    int active_threads_count = marker_count;

    while (active_threads_count > 0) {
        vector<HANDLE> current_dead_events;
        for (int i = 0; i < marker_count; i++) {
            if (!marker_finished[i]) {
                current_dead_events.push_back(hDeadEvents[i]);
            }
        }

        if (current_dead_events.empty()) {
            break;
        }

        WaitForMultipleObjects(current_dead_events.size(), current_dead_events.data(), TRUE, INFINITE);

        EnterCriticalSection(&cs);
        cout << "\n----------------------------------------" << endl;
        cout << "[main] Содержимое массива:" << endl;
        for (int i = 0; i < arr_size; i++) {
            cout << arr[i] << " ";
        }
        cout << "\n----------------------------------------" << endl;
        LeaveCriticalSection(&cs);

        int target_thread_num = 0;
        cout << "\nВведите порядковый номер потока marker для завершения: ";
        cin >> target_thread_num;

        int target_index = target_thread_num - 1;

        SetEvent(hKillEvents[target_index]);

        WaitForSingleObject(hThreads[target_index], INFINITE);

        EnterCriticalSection(&cs);
        cout << "\n[main] Содержимое массива после завершения потока " << target_thread_num << ":" << endl;
        for (int i = 0; i < arr_size; i++) {
            cout << arr[i] << " ";
        }
        cout << endl;
        LeaveCriticalSection(&cs);

        for (int i = 0; i < marker_count; i++) {
            if (!marker_finished[i] && i != target_index) {
                SetEvent(hContinueEvents[i]);
            }
        }

        active_threads_count--;
    }

    for (int i = 0; i < marker_count; i++) {
        CloseHandle(hThreads[i]);
        CloseHandle(hStartEvents[i]);
        CloseHandle(hContinueEvents[i]);
        CloseHandle(hKillEvents[i]);
        CloseHandle(hDeadEvents[i]);
    }

    delete[] arr;
    DeleteCriticalSection(&cs);

    cout << "\n[main] Все потоки завершены. Работа программы успешно завершена." << endl;
    return 0;
}