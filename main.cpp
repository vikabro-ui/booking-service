#include <iostream>
#include "httplib.h"
#include <nlohmann/json.hpp>

using json = nlohmann::json;

int main() {
    httplib::Server svr;
    svr.Get("/api/v1/rooms", [] (const httplib::Request& req, httplib::Response& res)
{
    json response;
    response["status"]="success";
    response["rooms"] = json::array({
            {{"id", 101}, {"name", "Переговорка 'Альфа'"}, {"capacity", 6}},
            {{"id", 102}, {"name", "Конференц-зал 'Бета'"}, {"capacity", 20}}
        });
    res.status = 200;
    res.set_content(response.dump(), "application/json");
    
});
    svr.listen("0.0.0.0", 8080);
    return 0;
}