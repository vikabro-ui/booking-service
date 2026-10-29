#include <exception>
#include <iostream>
#include "httplib.h"
#include <nlohmann/json.hpp>
#include <vector>
#include <algorithm> 
#include <pqxx/pqxx>

using json = nlohmann::json;

std::string get_db_connection_string() {
    return "dbname=booking_db user=postgres password=12082008 host=127.0.0.1 port=5432";
}

int main() {
    
    try {
        pqxx::connection conn(get_db_connection_string());
        if (conn.is_open()) {
            std::cout << "[DB] Успешное подключение к PostgreSQL! База: " << conn.dbname() << std::endl;
        }
    } catch (const std::exception &e) {
        std::cerr << "[DB Error] Ошибка подключения к базе данных: " << e.what() << std::endl;
        return 1;
    }

    httplib::Server svr;

    // Регистрация 
    svr.Post("/api/v1/register", [&](const httplib::Request& req, httplib::Response& res) {
        try {
            json request_data = json::parse(req.body);
            std::string username = request_data["username"];
            std::string password = request_data["password"];

            if(!request_data.contains("telegram_chat_id") || request_data["telegram_chat_id"].get<std::string>().empty()) {
                json error = {
                    {"status", "error"},
                    {"message", "Поле 'telegram_chat_id' является обязательным для регистрации!"}
                };
                res.status = 400;
                res.set_content(error.dump(4), "application/json");
                return;
            }

            std::string telegram_chat_id = request_data["telegram_chat_id"];
            pqxx::connection conn(get_db_connection_string());
            pqxx::work txn(conn);
            
            pqxx::result res_insert = txn.exec_params(
                "INSERT INTO users(username, password, telegram_chat_id) VALUES($1, $2, $3) RETURNING id;",
                username, password, telegram_chat_id
            );

            txn.commit();

            int user_id = res_insert[0]["id"].as<int>();
            json response = {
                {"status", "success"},
                {"message", "Регистрация прошла успешно!"},
                {"user_id", user_id}
            };
            res.status = 201;
            res.set_content(response.dump(4), "application/json");
        } catch(const std::exception &e) {
            json error = {{"status", "error"}, {"message", "Пользователь с таким именем уже существует или ошибка БД: " + std::string(e.what())}};
            res.status = 400;
            res.set_content(error.dump(4), "application/json");
        }
    });

    // Вход
    svr.Post("/api/v1/login", [&](const httplib::Request& req, httplib::Response& res) {
        try {
            json request_data = json::parse(req.body);
            std::string username = request_data["username"];
            std::string password = request_data["password"];

            pqxx::connection conn(get_db_connection_string());
            pqxx::work txn(conn);
            
            pqxx::result result = txn.exec_params(
                "SELECT id, password FROM users WHERE username = $1;",
                username
            );
            json response;
            if (!result.empty() && result[0]["password"].as<std::string>() == password) {
                response["status"] = "success";
                response["message"] = "Вход выполнен успешно!";
                response["user_id"] = result[0]["id"].as<int>();
                res.status = 200;
            } else {
                response["status"] = "error";
                response["message"] = "Неверное имя пользователя или пароль!";
                res.status = 401; // Unauthorized
            }
            res.set_content(response.dump(4), "application/json");
        } catch(const std::exception &e) {
            json error = {{"status", "error"}, {"message", e.what()}};
            res.status = 500;
            res.set_content(error.dump(4), "application/json");
        }
    });

    // Получить список комнат (с фильтром min_capacity)
    svr.Get("/api/v1/rooms", [](const httplib::Request& req, httplib::Response& res) {
        try {
            pqxx::connection conn(get_db_connection_string());
            pqxx::work txn(conn);

            pqxx::result result;

            if (req.has_param("min_capacity")) {
                std::string min_cap_str = req.get_param_value("min_capacity");
                int min_capacity = std::stoi(min_cap_str);

                result = txn.exec_params(
                    "SELECT id, name, capacity FROM rooms WHERE capacity >= $1;",
                    min_capacity
                );
            } else {
                result = txn.exec("SELECT id, name, capacity FROM rooms;");
            }

            json rooms_array = json::array();
            for (auto row : result) {
                json room;
                room["id"] = row["id"].as<int>();
                room["name"] = row["name"].as<std::string>();
                room["capacity"] = row["capacity"].as<int>();
                rooms_array.push_back(room);
            }

            res.set_content(rooms_array.dump(4), "application/json");
        } catch (const std::exception &e) {
            json error = {{"error", e.what()}};
            res.status = 500;
            res.set_content(error.dump(), "application/json");
        }
    });
  
    // Получить список бронирований (всех или с фильтром по интервалу времени)
    svr.Get("/api/v1/bookings", [&](const httplib::Request& req, httplib::Response& res) {
        try {
            pqxx::connection conn(get_db_connection_string());
            pqxx::work txn(conn);
            pqxx::result result;

            std::string base_query =
                "SELECT b.id, b.room_id, r.name as room_name, b.user_id, u.username, b.start_hour, b.end_hour "
                "FROM bookings b "
                "JOIN rooms r ON b.room_id = r.id "
                "JOIN users u ON b.user_id = u.id";

            if(req.has_param("start_hour") && req.has_param("end_hour")) {
                int q_start = std::stoi(req.get_param_value("start_hour"));
                int q_end = std::stoi(req.get_param_value("end_hour"));
                result = txn.exec_params(base_query + " WHERE $1 < b.end_hour AND b.start_hour < $2;", q_start, q_end);
            } else { 
                result = txn.exec(base_query + ";"); 
            }

            json bookings_array = json::array();
            for (auto row : result) {
                json booking;
                booking["id"] = row["id"].as<int>();
                booking["room_id"] = row["room_id"].as<int>();
                booking["room_name"] = row["room_name"].as<std::string>();
                booking["user_id"] = row["user_id"].as<int>();
                booking["username"] = row["username"].as<std::string>();
                booking["start_hour"] = row["start_hour"].as<int>();
                booking["end_hour"] = row["end_hour"].as<int>();
                bookings_array.push_back(booking);
            }

            json response = {
                {"status", "success"},
                {"bookings", bookings_array}
            };
            res.status = 200;
            res.set_content(response.dump(4), "application/json");
        } catch(const std::exception &e) { 
            json error = {{"error", e.what()}};
            res.status = 500;
            res.set_content(error.dump(4), "application/json");
        }
    });

    // Получить историю бронирования конкретного пользователя 
    svr.Get("/api/v1/users/:id/bookings", [&](const httplib::Request& req, httplib::Response& res) {
        try {
            int user_id = std::stoi(req.path_params.at("id"));
            pqxx::connection conn(get_db_connection_string());
            pqxx::work txn(conn);
            pqxx::result user_check = txn.exec_params("SELECT id FROM users WHERE id = $1;", user_id);

            if (user_check.empty()) {
                json error = {{"status", "error"}, {"message", "Пользователь с таким ID не найден!"}};
                res.status = 404;
                res.set_content(error.dump(4), "application/json");
                return;
            }

            pqxx::result result = txn.exec_params(
                "SELECT b.id, b.room_id, r.name as room_name, b.user_id, u.username, b.start_hour, b.end_hour "
                "FROM bookings b "
                "JOIN rooms r ON b.room_id = r.id "
                "JOIN users u ON b.user_id = u.id "
                "WHERE b.user_id = $1;", 
                user_id
            );

            json user_bookings = json::array();
            for (auto row : result) {
                json booking;
                booking["id"] = row["id"].as<int>();
                booking["room_id"] = row["room_id"].as<int>();
                booking["room_name"] = row["room_name"].as<std::string>();
                booking["user_id"] = row["user_id"].as<int>();
                booking["username"] = row["username"].as<std::string>();
                booking["start_hour"] = row["start_hour"].as<int>();
                booking["end_hour"] = row["end_hour"].as<int>();
                user_bookings.push_back(booking);
            }

            json response = {
                {"status", "success"},
                {"user_id", user_id},
                {"bookings", user_bookings}
            };
            res.status = 200;
            res.set_content(response.dump(4), "application/json");
        } catch (const std::exception &e) {
            json error = {{"error", e.what()}};
            res.status = 500;
            res.set_content(error.dump(4), "application/json");
        }
    });

    // Создать бронирование 
    svr.Post("/api/v1/bookings", [&](const httplib::Request& req, httplib::Response& res) {
        try {
            json request_data = json::parse(req.body);
            int room_id = request_data["room_id"];
            int user_id = request_data["user_id"]; 
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

            pqxx::connection conn(get_db_connection_string());
            pqxx::work txn(conn);
            
            pqxx::result room_check = txn.exec_params("SELECT id FROM rooms WHERE id = $1;", room_id);
            if(room_check.empty()) {
                response["status"] = "error";
                response["message"] = "Комната не найдена!";
                res.status = 400;
                res.set_content(response.dump(), "application/json");
                return;
            }

            pqxx::result user_check = txn.exec_params("SELECT id FROM users WHERE id = $1;", user_id);
            if(user_check.empty()) {
                response["status"] = "error";
                response["message"] = "Пользователь не найден!";
                res.status = 400;
                res.set_content(response.dump(), "application/json");
                return;
            }
            
            pqxx::result overlap_check = txn.exec_params(
                "SELECT id FROM bookings WHERE room_id = $1 AND $2 < end_hour AND start_hour < $3;",
                room_id, start_hour, end_hour
            );

            if (!overlap_check.empty()) {
                response["status"] = "error";
                response["message"] = "Этот временной интервал уже забронирован!";
                res.status = 409; // Conflict
                res.set_content(response.dump(4), "application/json");
                return;
            }

            pqxx::result insert_res = txn.exec_params(
                "INSERT INTO bookings (room_id, user_id, start_hour, end_hour) VALUES ($1, $2, $3, $4) RETURNING id;",
                room_id, user_id, start_hour, end_hour
            );
            txn.commit();

            int booking_id = insert_res[0]["id"].as<int>();
            
            json new_booking = {
                {"id", booking_id}, 
                {"room_id", room_id},
                {"user_id", user_id},
                {"start_hour", start_hour},
                {"end_hour", end_hour}
            };

            response["status"] = "success";
            response["message"] = "Комната успешно забронирована!";
            response["booking_details"] = new_booking;
            res.status = 201;
            res.set_content(response.dump(4), "application/json");
        } catch (const std::exception &e) {
            json error = {{"error", e.what()}};
            res.status = 500;
            res.set_content(error.dump(4), "application/json");
        }
    });

    // Получить конкретное бронирование по ID
    svr.Get("/api/v1/bookings/:id", [](const httplib::Request& req, httplib::Response& res) {
        try {
            int booking_id = std::stoi(req.path_params.at("id"));

            pqxx::connection conn(get_db_connection_string());
            pqxx::work txn(conn);

            pqxx::result result = txn.exec_params(
                "SELECT b.id, b.room_id, r.name as room_name, b.user_id, u.username, b.start_hour, b.end_hour "
                "FROM bookings b "
                "JOIN rooms r ON b.room_id = r.id "
                "JOIN users u ON b.user_id = u.id "
                "WHERE b.id = $1;",
                booking_id
            );

            json response;
            if (!result.empty()) {
                json booking;
                booking["id"] = result[0]["id"].as<int>();
                booking["room_id"] = result[0]["room_id"].as<int>();
                booking["room_name"] = result[0]["room_name"].as<std::string>();
                booking["user_id"] = result[0]["user_id"].as<int>();
                booking["username"] = result[0]["username"].as<std::string>();
                booking["start_hour"] = result[0]["start_hour"].as<int>();
                booking["end_hour"] = result[0]["end_hour"].as<int>();

                response["status"] = "success";
                response["message"] = "Бронирование найдено!";
                response["booking_details"] = booking;
                res.status = 200;
            } else {
                response["status"] = "error";
                response["message"] = "Бронирование не найдено!";
                res.status = 404;
            }
            res.set_content(response.dump(4), "application/json");
        } catch (const std::exception &e) {
            json error = {{"error", e.what()}};
            res.status = 500;
            res.set_content(error.dump(4), "application/json");
        }
    });

    // Обновить существующее бронирование
    svr.Put("/api/v1/bookings/:id", [](const httplib::Request& req, httplib::Response& res) {
        try {
           
            int booking_id = std::stoi(req.path_params.at("id"));
            json request_data = json::parse(req.body);

            int room_id = request_data.at("room_id");
            int user_id = request_data.at("user_id");
            int start_hour = request_data.at("start_hour");
            int end_hour = request_data.at("end_hour");

            pqxx::connection conn(get_db_connection_string());
            pqxx::work txn(conn);

            txn.exec_params(
                "UPDATE bookings SET room_id = $1, user_id = $2, start_hour = $3, end_hour = $4 WHERE id = $5;",
                room_id, user_id, start_hour, end_hour, booking_id
            );
            txn.commit();

            pqxx::result result = txn.exec_params(
                "SELECT b.id, b.room_id, r.name as room_name, b.user_id, u.username, b.start_hour, b.end_hour "
                "FROM bookings b "
                "JOIN rooms r ON b.room_id = r.id "
                "JOIN users u ON b.user_id = u.id "
                "WHERE b.id = $1;",
                booking_id
            );

            json booking;
            if (!result.empty()) {
                booking["id"] = result[0]["id"].as<int>();
                booking["room_id"] = result[0]["room_id"].as<int>();
                booking["room_name"] = result[0]["room_name"].as<std::string>();
                booking["user_id"] = result[0]["user_id"].as<int>();
                booking["username"] = result[0]["username"].as<std::string>();
                booking["start_hour"] = result[0]["start_hour"].as<int>();
                booking["end_hour"] = result[0]["end_hour"].as<int>();
            }

            json response = {
                {"status", "success"},
                {"message", "Бронирование обновлено!"},
                {"booking_details", booking}
            };
            res.status = 200;
            res.set_content(response.dump(4), "application/json");

        } catch (const std::exception &e) {
            json error = {{"status", "error"}, {"message", e.what()}};
            res.status = 500;
            res.set_content(error.dump(4), "application/json");
        }
    });

    // Удалить бронирование
    svr.Delete("/api/v1/bookings/:id", [](const httplib::Request& req, httplib::Response& res) {
        try {
            
            int booking_id = std::stoi(req.path_params.at("id"));

            pqxx::connection conn(get_db_connection_string());
            pqxx::work txn(conn);

            pqxx::result check = txn.exec_params(
                "SELECT b.id, b.room_id, r.name as room_name, b.user_id, u.username, b.start_hour, b.end_hour "
                "FROM bookings b "
                "JOIN rooms r ON b.room_id = r.id "
                "JOIN users u ON b.user_id = u.id "
                "WHERE b.id = $1;",
                booking_id
            );

            if (check.empty()) {
                json error = {{"status", "error"}, {"message", "Бронирование с таким ID не найдено."}};
                res.status = 404;
                res.set_content(error.dump(4), "application/json");
                return;
            }

            json deleted_booking = {
                {"id", check[0]["id"].as<int>()},
                {"room_id", check[0]["room_id"].as<int>()},
                {"room_name", check[0]["room_name"].as<std::string>()},
                {"user_id", check[0]["user_id"].as<int>()},
                {"username", check[0]["username"].as<std::string>()},
                {"start_hour", check[0]["start_hour"].as<int>()},
                {"end_hour", check[0]["end_hour"].as<int>()}
            };

            txn.exec_params("DELETE FROM bookings WHERE id = $1;", booking_id);
            txn.commit();

            json response = {
                {"status", "success"},
                {"message", "Бронирование успешно удалено!"},
                {"deleted_booking", deleted_booking}
            };
            res.status = 200;
            res.set_content(response.dump(4), "application/json");

        } catch (const std::exception &e) {
            json error = {{"status", "error"}, {"message", e.what()}};
            res.status = 500;
            res.set_content(error.dump(4), "application/json");
        }
    });

    svr.listen("0.0.0.0", 8080);
    return 0;
}