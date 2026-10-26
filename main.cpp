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


    std::vector<json> bookings_storage;
    int next_booking_id = 1; 

    httplib::Server svr;

    //получить список всех комнат
    svr.Get("/api/v1/rooms", [&](const httplib::Request& req, httplib::Response& res) {
        json response;
        response["status"] = "success";
        response["rooms"] = rooms_storage;
        res.status = 200;
        res.set_content(response.dump(), "application/json");
    });

    //создать бронирование 
    svr.Post("/api/v1/bookings", [&](const httplib::Request& req, httplib::Response& res) {
        json request_data = json::parse(req.body);
        int roomId = request_data["room_id"];
        std::string userName = request_data["user_name"];

        auto room_it = std::find_if(rooms_storage.begin(), rooms_storage.end(), [roomId](const json& room) {
            return room["id"] == roomId;
        });

        json response;
        if (room_it != rooms_storage.end()) {
            int current_id = next_booking_id++;
            
            json new_booking = {
                {"id", current_id}, 
                {"room_id", roomId},
                {"user_name", userName}
            };

            bookings_storage.push_back(new_booking);

            response["status"] = "success";
            response["message"] = "Комната успешно забронирована!";
            response["booking_details"] = new_booking;
            res.status = 201;
        } else {
            response["status"] = "error";
            response["message"] = "Такой комнаты не найдено!";
            res.status = 404;
        }

        res.set_content(response.dump(), "application/json");
    });

    // получить список всех бронирований
    svr.Get("/api/v1/bookings", [&](const httplib::Request& req, httplib::Response& res) {
        json response;
        response["status"] = "success";
        response["bookings"] = bookings_storage; 

        res.status = 200;
        res.set_content(response.dump(), "application/json");
    });

    //получить конкретное бронирование по ID
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

    // обновить существующее бронирование
    svr.Put("/api/v1/bookings/:id", [&](const httplib::Request& req, httplib::Response& res) {
        int booking_id = std::stoi(req.path_params.at("id"));
        json request_data = json::parse(req.body);

        auto it = std::find_if(bookings_storage.begin(), bookings_storage.end(), [booking_id](const json& b) {
            return b["id"] == booking_id;
        });

        json response;
        if (it != bookings_storage.end()) {
            if (request_data.contains("room_id")) {
                int new_room_id = request_data["room_id"];
                (*it)["room_id"] = new_room_id;
            }

            if (request_data.contains("user_name")) {
                (*it)["user_name"] = request_data["user_name"];
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

    // удалить бронирование
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