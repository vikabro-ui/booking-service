#include <iostream>
#include "httplib.h"
#include <nlohmann/json.hpp>
#include <vector>
#include <algorithm> 

using json = nlohmann::json;

int main() {

    std::vector<json> rooms_storage = {
        {{"id", 101}, {"name", "Переговорка 'Альфа'"}, {"capacity", 6}},
        {{"id", 102}, {"name", "Конференц-зал 'Бета'"}, {"capacity", 20}}
    };

    std::vector<json> users_storage;
    int next_user_id = 1;
    std::vector<json> bookings_storage;
    int next_booking_id = 1; 

    httplib::Server svr;

    // Регистрация 
    svr.Post("/api/v1/register", [&](const httplib::Request& req, httplib::Response& res) {
        json request_data = json::parse(req.body);
        std::string username = request_data["username"];
        std::string password = request_data["password"];

        std::string telegram_chat_id = "";
        if (request_data.contains("telegram_chat_id")) {
            telegram_chat_id = request_data["telegram_chat_id"];
        }

        json response;

        auto it = std::find_if(users_storage.begin(), users_storage.end(), [username](const json& user) {
            return user["username"] == username;
        });

        if (it != users_storage.end()) {
            response["status"] = "error";
            response["message"] = "Пользователь с таким именем уже существует!";
            res.status = 400;
        } else {
            int current_id = next_user_id++;
            
            json new_user = {
                {"id", current_id},
                {"username", username},
                {"password", password},
                {"telegram_chat_id", telegram_chat_id} 
            };

            users_storage.push_back(new_user);

            response["status"] = "success";
            response["message"] = "Регистрация прошла успешно!";
            response["user_id"] = current_id;
            res.status = 201;
        }

        res.set_content(response.dump(), "application/json");
    });

    // Вход
    svr.Post("/api/v1/login", [&](const httplib::Request& req, httplib::Response& res) {
        json request_data = json::parse(req.body);
        std::string username = request_data["username"];
        std::string password = request_data["password"];

        json response;

        auto it = std::find_if(users_storage.begin(), users_storage.end(), [username](const json& user) {
            return user["username"] == username;
        });

        if (it != users_storage.end() && (*it)["password"] == password) {
            response["status"] = "success";
            response["message"] = "Вход выполнен успешно!";
            response["user_id"] = (*it)["id"]; 
            res.status = 200;
        } else {
            response["status"] = "error";
            response["message"] = "Неверное имя пользователя или пароль!";
            res.status = 401; 
        }

        res.set_content(response.dump(), "application/json");
    });

    // Получить список комнат (с фильтром min_capacity)
    svr.Get("/api/v1/rooms", [&](const httplib::Request& req, httplib::Response& res) {
        json response = json::array();

        if (req.has_param("min_capacity")) {
            int min_cap = std::stoi(req.get_param_value("min_capacity"));
            for (const auto& room : rooms_storage) {
                if (room["capacity"] >= min_cap) {
                    response.push_back(room);
                }
            }
        } else {
            for (const auto& room : rooms_storage) {
                response.push_back(room);
            }
        }

        json final_response;
        final_response["status"] = "success";
        final_response["rooms"] = response;

        res.status = 200;
        res.set_content(final_response.dump(), "application/json");
    });
  
    // Получить список бронирований (всех или с фильтром по интервалу времени)
    svr.Get("/api/v1/bookings", [&](const httplib::Request& req, httplib::Response& res) {
        json response;

        if (req.has_param("start_hour") && req.has_param("end_hour")) {
            int query_start = std::stoi(req.get_param_value("start_hour"));
            int query_end = std::stoi(req.get_param_value("end_hour"));

            json filtered_bookings = json::array();

            for (const auto& booking : bookings_storage) {
                int b_start = booking["start_hour"];
                int b_end = booking["end_hour"];

                if (query_start < b_end && b_start < query_end) {
                    filtered_bookings.push_back(booking);
                }
            }

            response["status"] = "success";
            response["filter"] = {{"start_hour", query_start}, {"end_hour", query_end}};
            response["bookings"] = filtered_bookings;
            res.status = 200;
            res.set_content(response.dump(), "application/json");
            return;
        }

        response["status"] = "success";
        response["bookings"] = bookings_storage; 
        res.status = 200;
        res.set_content(response.dump(), "application/json");
    });

    // Получить историю бронирования конкретного пользователя 
    svr.Get("/api/v1/users/:id/bookings", [&](const httplib::Request& req, httplib::Response& res) {
        json response;
        int user_id = std::stoi(req.path_params.at("id"));
        
        auto user_it = std::find_if(users_storage.begin(), users_storage.end(), [user_id](const json& user) {
            return user["id"] == user_id;
        });

        if (user_it == users_storage.end()) {
            response["status"] = "error";
            response["message"] = "Пользователь с таким ID не найден!";
            res.status = 404;
            res.set_content(response.dump(), "application/json");
            return; 
        }

        std::vector<json> user_bookings;
        for (const auto& booking : bookings_storage) {
            if (booking["user_id"] == user_id) {
                user_bookings.push_back(booking);
            }
        }

        response["status"] = "success";
        response["user_id"] = user_id;
        response["bookings"] = user_bookings;
        res.status = 200;

        res.set_content(response.dump(), "application/json");
    });

    // Создать бронирование 
    svr.Post("/api/v1/bookings", [&](const httplib::Request& req, httplib::Response& res) {
        json request_data = json::parse(req.body);
        int roomId = request_data["room_id"];
        int userId = request_data["user_id"]; 

        int start_hour = request_data.contains("start_hour") ? request_data["start_hour"].get<int>() : 0;
        int end_hour = request_data.contains("end_hour") ? request_data["end_hour"].get<int>() : 24;

        json response;

        if (start_hour >= end_hour || start_hour < 0 || end_hour > 24) {
            response["status"] = "error";
            response["message"] = "Некорректный временной интервал!";
            res.status = 400;
            res.set_content(response.dump(), "application/json");
            return;
        }

        auto room_it = std::find_if(rooms_storage.begin(), rooms_storage.end(), [roomId](const json& room) {
            return room["id"] == roomId;
        });
        auto user_it = std::find_if(users_storage.begin(), users_storage.end(), [userId](const json& user) {
            return user["id"] == userId;
        });

        if (room_it == rooms_storage.end()) {
            response["status"] = "error";
            response["message"] = "Такой комнаты не существует!";
            res.status = 404;
            res.set_content(response.dump(), "application/json");
            return;
        }

        if (user_it == users_storage.end()) {
            response["status"] = "error";
            response["message"] = "Пользователь с таким ID не найден!";
            res.status = 404;
            res.set_content(response.dump(), "application/json");
            return;
        }

        for (const auto& booking : bookings_storage) {
            if (booking["room_id"] == roomId) {
                int existing_start = booking["start_hour"];
                int existing_end = booking["end_hour"];

                if (start_hour < existing_end && existing_start < end_hour) {
                    response["status"] = "error";
                    response["message"] = "Этот временной интервал уже забронирован!";
                    res.status = 409; 
                    res.set_content(response.dump(), "application/json");
                    return;
                }
            }
        }
    
        int current_id = next_booking_id++;
        
        json new_booking = {
            {"id", current_id}, 
            {"room_id", roomId},
            {"user_id", userId},
            {"start_hour", start_hour},
            {"end_hour", end_hour}
        };

        bookings_storage.push_back(new_booking);

        response["status"] = "success";
        response["message"] = "Комната успешно забронирована!";
        response["booking_details"] = new_booking;
        res.status = 201;

        res.set_content(response.dump(), "application/json");
    });

    // Получить конкретное бронирование по ID
    svr.Get("/api/v1/bookings/:id", [&](const httplib::Request& req, httplib::Response& res) {
        int booking_id = std::stoi(req.path_params.at("id"));

        auto it = std::find_if(bookings_storage.begin(), bookings_storage.end(), [booking_id](const json& b) {
            return b["id"] == booking_id;
        }); 

        json response;
        if (it != bookings_storage.end()) {
            response["status"] = "success";
            response["message"] = "Бронирование найдено!";
            response["booking_details"] = *it;
            res.status = 200;
        } else {
            response["status"] = "error";
            response["message"] = "Бронирование не найдено!";
            res.status = 404;
        }

        res.set_content(response.dump(), "application/json");
    });

    // Обновить существующее бронирование
    svr.Put("/api/v1/bookings/:id", [&](const httplib::Request& req, httplib::Response& res) {
        int booking_id = std::stoi(req.path_params.at("id"));
        json request_data = json::parse(req.body);

        auto it = std::find_if(bookings_storage.begin(), bookings_storage.end(), [booking_id](const json& b) {
            return b["id"] == booking_id;
        });

        json response;
        if (it != bookings_storage.end()) {
            if (request_data.contains("room_id")) {
                (*it)["room_id"] = request_data["room_id"];
            }
            if (request_data.contains("user_id")) {
                (*it)["user_id"] = request_data["user_id"];
            }
            if (request_data.contains("start_hour")) {
                (*it)["start_hour"] = request_data["start_hour"];
            }
            if (request_data.contains("end_hour")) {
                (*it)["end_hour"] = request_data["end_hour"];
            }

            response["status"] = "success";
            response["message"] = "Бронирование успешно обновлено!";
            response["booking_details"] = *it;
            res.status = 200;
        } else {
            response["status"] = "error";
            response["message"] = "Бронирование с таким ID не найдено.";
            res.status = 404;
        }

        res.set_content(response.dump(), "application/json");
    });

    // Удалить бронирование
    svr.Delete("/api/v1/bookings/:id", [&](const httplib::Request& req, httplib::Response& res) {
        int booking_id = std::stoi(req.path_params.at("id"));
        
        auto it = std::remove_if(bookings_storage.begin(), bookings_storage.end(), [booking_id](const json& b) {
            return b["id"] == booking_id;
        });

        json response;
        if (it != bookings_storage.end()) {
            bookings_storage.erase(it, bookings_storage.end());
            response["status"] = "success";
            response["message"] = "Бронирование с ID " + std::to_string(booking_id) + " успешно удалено.";
            res.status = 200;
        } else {
            response["status"] = "error";
            response["message"] = "Бронирование с таким ID не найдено.";
            res.status = 404;
        }

        res.set_content(response.dump(), "application/json");
    });

    svr.listen("0.0.0.0", 8080);
    return 0;
}