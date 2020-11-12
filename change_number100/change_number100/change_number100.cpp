#include <iostream>
#include <uwebsockets/App.h>
#include <atomic>
#include <thread>
#include <algorithm>
using namespace std;

template <class K, class V, class Compare = less<K>, class Allocator = allocator<pair<const K, V>>>
class syncr_map
{
private:
    map<K, V, Compare, Allocator> _map;
    mutex _mutex;
public:
    void set(K key, V value) {
        lock_guard<mutex> lg(this->_mutex);
        this->_map[key] = value;
    }

    V get(K key) {
        lock_guard<mutex> lg(this->_mutex);
        return this->_map[key];
    }

    vector<string> getExistingUsers() {
        lock_guard<mutex> lg(this->_mutex);
        vector<string> result;
        for (auto entry : this->_map)
        {
            result.push_back("NEW_USER," /*+ entry.second + "," + entry.first*/);
        }
        return result;
        // string broadcast_message = "NEW_USER," + data->user_name + "," + to_string(data->user_id);
    }
};

struct UserConnection {
    unsigned long user_id; 
    string user_name;
};

syncr_map<unsigned long, string> user_names;


int main()
{

    atomic_ulong latest_user_id = 10;
    // ws://127.0.0.1:9999/
    vector<thread*> threads(thread::hardware_concurrency()); // запускаю кол-во потоков, которое равно количество ядер в моем компьютере
    transform(threads.begin(), threads.end(), threads.begin(), [&latest_user_id](thread* thr) {
        return new thread([&latest_user_id]() {
            //Что делает данный поток
            uWS::App().ws<UserConnection>("/*", { // создали приложение, сервер
            //что делать серверу в разных ситуациях
            .open = [&latest_user_id](auto* ws) { // функция, которая вызывается в том момент, когда открывается новое соединение (получаемый аргумент)
                UserConnection* data = (UserConnection*)ws->getUserData();
                data->user_id = latest_user_id++;
                cout << "Id of new user = " << data->user_id << endl;
                ws->subscribe("broadcast");
                ws->subscribe("user#" + to_string(data->user_id));
                data->user_name = "Unnames User";
                user_names.set(data->user_id, data->user_name);

                // Сообщаем новому пользователю о уже подключенных юзерах
                for (string entry : user_names.getExistingUsers())
                {
                    ws->send(entry, uWS::OpCode::TEXT, false);

                }
            },

            .message = [](auto* ws, string_view message, uWS::OpCode opCode) {
                UserConnection* data = (UserConnection*)ws->getUserData();
                string_view beginning = message.substr(0,10);
                if (beginning.compare("USER_NAME=") == 0) // compare возвращает 0, если нашел совпадение
                {
                    string_view name = message.substr(10);
                    data->user_name = string(name);
                    cout << "User said their name ( ID = " << data->user_id << " ), name = " << data->user_name << endl;
                    //уведомление о новом участнике
                    string broadcast_message = "NEW_USER," + data->user_name + "," + to_string(data->user_id);
                    ws->publish("broadcast", string_view(broadcast_message), opCode, false);
                    user_names.set(data->user_id, data->user_name);
                }
                // MESSAGE_TO,11,message
                string_view is_message_to = message.substr(0, 11);
                if (is_message_to.compare("MESSAGE_TO,") == 0)
                {
                    string_view rest = message.substr(11); // id, message
                    int comma_position = rest.find(",");
                    string_view id_string = rest.substr(0, comma_position); // id
                    string_view user_message = rest.substr(comma_position + 1); // message
                    ws->publish("user#" + string(id_string), user_message, opCode, false);
                    cout << "New private message" << message << endl;
                }
            }
            })
            .listen(9999, [](auto* token) {
                if (token) {
                    cout << "The token came, everything is fine and the server started\n";
                }
                else {
                    cout << "Something went wrong\n";
                }

            })
            .run();
        });
    });
    for_each(threads.begin(), threads.end(), [](thread* thr) 
    {
        thr->join();
    });
}