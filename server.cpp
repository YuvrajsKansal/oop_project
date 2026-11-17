#include "httplib.h"
#include <iostream>
#include <string>
#include <fstream>
#include <cctype>
#include <cstdlib>
#include <ctime>
#include <mutex>
using namespace std;

string toLower(string s) {
    for (int i = 0; i < s.length(); i++) {
        s[i] = tolower(s[i]);
    }
    return s;
}

string jsonStr(string s) {
    string out = "\"";
    for (int i = 0; i < s.length(); i++) {
        char c = s[i];
        if (c == '"') out += "\\\"";
        else if (c == '\\') out += "\\\\";
        else if (c == '\n') out += "\\n";
        else if (c == '\r') out += "\\r";
        else if (c == '\t') out += "\\t";
        else if ((unsigned char)c < 32) out += " ";
        else out += c;
    }
    out += "\"";
    return out;
}

class Item {
private:
    static int nextId;

protected:
    int id;
    string name;
    string date;
    int reporterID;
    string description;
    string category;
    string location;
    bool isResolved;

public:
    Item(string n, string d, int rid, string desc, string cat, string loc) {
        id = nextId++;
        name = n;
        date = d;
        reporterID = rid;
        description = desc;
        category = cat;
        location = loc;
        isResolved = false;
    }

    virtual ~Item() {}

    virtual string getType() = 0;
    virtual string toJson(string reporterName) = 0;

    virtual void saveToFile(ofstream& file) {
        file << id << "\n";
        file << name << "\n";
        file << description << "\n";
        file << category << "\n";
        file << location << "\n";
        file << date << "\n";
        file << reporterID << "\n";
        file << (isResolved ? 1 : 0) << "\n";
    }

    string baseJson() {
        return "\"id\":" + to_string(id) +
               ",\"name\":" + jsonStr(name) +
               ",\"description\":" + jsonStr(description) +
               ",\"category\":" + jsonStr(category) +
               ",\"place\":" + jsonStr(location) +
               ",\"date\":" + jsonStr(date) +
               ",\"reporter\":" + to_string(reporterID) +
               ",\"resolved\":" + (isResolved ? "true" : "false");
    }

    bool matches(string keyword) {
        string all = toLower(name + " " + description + " " + category + " " + location);
        string word = toLower(keyword);

        int position = all.find(word);

        if (position == -1) {
            return false;
        }
        else {
            return true;
        }
    }

    void markResolved(){
         isResolved = true; 
    }

    void setId(int newId){
         id = newId; 
    }

    static void updateNextId(int usedId){
         if (usedId >= nextId) {
             nextId = usedId + 1;
         }
    }

    int getId(){
         return id; 
    }
    int getReporterId(){
         return reporterID; 
    }
};

int Item::nextId = 1;

class LostItem : public Item {
private:
    string ownerContact;
    string identifyingMark;

public:
    LostItem(string n, string d, int rid, string desc, string cat, string loc,
             string ocontact, string mark)
        : Item(n, d, rid, desc, cat, loc) {
        ownerContact = ocontact;
        identifyingMark = mark;
    }

    string getType() override {
        return "lost";
    }

    string toJson(string reporterName) override {
        return "{\"type\":\"lost\"," + baseJson() +
               ",\"contact\":" + jsonStr(ownerContact) +
               ",\"extra\":" + jsonStr(identifyingMark) +
               ",\"reporterName\":" + jsonStr(reporterName) + "}";
    }

    void saveToFile(ofstream& file) override {
        Item::saveToFile(file);
        file << ownerContact << "\n";
        file << identifyingMark << "\n";
    }
};

class FoundItem : public Item {
private:
    string finderContact;
    string whereKept;

public:
    FoundItem(string n, string d, int rid, string desc, string cat, string loc,
              string fcontact, string kept)
        : Item(n, d, rid, desc, cat, loc) {
        finderContact = fcontact;
        whereKept = kept;
    }

    string getType() override {
        return "found";
    }

    string toJson(string reporterName) override {
        return "{\"type\":\"found\"," + baseJson() +
               ",\"contact\":" + jsonStr(finderContact) +
               ",\"extra\":" + jsonStr(whereKept) +
               ",\"reporterName\":" + jsonStr(reporterName) + "}";
    }

    void saveToFile(ofstream& file) override {
        Item::saveToFile(file);
        file << finderContact << "\n";
        file << whereKept << "\n";
    }
};

class User {
private:
    static int nextUserId;
    int userId;
    string username;
    string password;
    string contact;

public:
    User(string u, string p, string c) {
        userId = nextUserId++;
        username = u;
        password = p;
        contact = c;
    }

    bool checkPassword(string p) {
        return password == p;
    }

    void saveToFile(ofstream& file) {
        file << username << "\n";
        file << password << "\n";
        file << contact << "\n";
    }

    int getUserId() { return userId; }
    string getUsername() { return username; }
    string getContact() { return contact; }
};

int User::nextUserId = 1;

LostItem* lostItems[100];
FoundItem* foundItems[100];
User* users[100];
int lostCount = 0;
int foundCount = 0;
int userCount = 0;

string tokens[200];
User* tokenOwner[200];
int tokenCount = 0;

mutex dataMutex;

string categories[6] = {"Wallet", "Phone", "ID Card", "Bag", "Keys", "Other"};

bool validCategory(string c) {
    for (int i = 0; i < 6; i++) {
        if (categories[i] == c) {
            return true;
        }
    }
    return false;
}

void saveAll() {
    ofstream userFile("users.txt");
    userFile << userCount << "\n";
    for (int i = 0; i < userCount; i++) {
        users[i]->saveToFile(userFile);
    }

    ofstream lostFile("lost.txt");
    lostFile << lostCount << "\n";
    for (int i = 0; i < lostCount; i++) {
        lostItems[i]->saveToFile(lostFile);
    }

    ofstream foundFile("found.txt");
    foundFile << foundCount << "\n";
    for (int i = 0; i < foundCount; i++) {
        foundItems[i]->saveToFile(foundFile);
    }
}

void loadAll() {
    ifstream userFile("users.txt");
    if (userFile) {
        int count = 0;
        userFile >> count;
        userFile.ignore();
        for (int i = 0; i < count && userCount < 100; i++) {
            string username, password, contact;
            getline(userFile, username);
            getline(userFile, password);
            getline(userFile, contact);

            users[userCount] = new User(username, password, contact);
            userCount++;
        }
    }

    ifstream lostFile("lost.txt");
    if (lostFile) {
        int count = 0;
        lostFile >> count;
        lostFile.ignore();
        for (int i = 0; i < count && lostCount < 100; i++) {
            int id, reporterId, resolved;
            string name, description, category, location, date, contact, mark;

            lostFile >> id;
            lostFile.ignore();
            getline(lostFile, name);
            getline(lostFile, description);
            getline(lostFile, category);
            getline(lostFile, location);
            getline(lostFile, date);
            lostFile >> reporterId;
            lostFile >> resolved;
            lostFile.ignore();
            getline(lostFile, contact);
            getline(lostFile, mark);

            LostItem* item = new LostItem(name, date, reporterId, description,
                                          category, location, contact, mark);
            item->setId(id);
            Item::updateNextId(id);
            if (resolved == 1) {
                item->markResolved();
            }
            lostItems[lostCount] = item;
            lostCount++;
        }
    }

    ifstream foundFile("found.txt");
    if (foundFile) {
        int count = 0;
        foundFile >> count;
        foundFile.ignore();
        for (int i = 0; i < count && foundCount < 100; i++) {
            int id, reporterId, resolved;
            string name, description, category, location, date, contact, kept;

            foundFile >> id;
            foundFile.ignore();
            getline(foundFile, name);
            getline(foundFile, description);
            getline(foundFile, category);
            getline(foundFile, location);
            getline(foundFile, date);
            foundFile >> reporterId;
            foundFile >> resolved;
            foundFile.ignore();
            getline(foundFile, contact);
            getline(foundFile, kept);

            FoundItem* item = new FoundItem(name, date, reporterId, description,
                                            category, location, contact, kept);
            item->setId(id);
            Item::updateNextId(id);
            if (resolved == 1) {
                item->markResolved();
            }
            foundItems[foundCount] = item;
            foundCount++;
        }
    }
}

int findUser(string username) {
    for (int i = 0; i < userCount; i++) {
        if (users[i]->getUsername() == username) {
            return i;
        }
    }
    return -1;
}

User* findUserById(int id) {
    for (int i = 0; i < userCount; i++) {
        if (users[i]->getUserId() == id) {
            return users[i];
        }
    }
    return nullptr;
}

string makeToken() {
    string chars = "abcdefghijklmnopqrstuvwxyz0123456789";
    string token = "";
    for (int i = 0; i < 24; i++) {
        token += chars[rand() % chars.length()];
    }
    return token;
}

User* getCurrentUser(const httplib::Request& req) {
    string token = req.get_header_value("X-Token");
    if (token.empty()) {
        return nullptr;
    }
    int n = tokenCount < 200 ? tokenCount : 200;
    for (int i = 0; i < n; i++) {
        if (tokens[i] == token) {
            return tokenOwner[i];
        }
    }
    return nullptr;
}

void sendJson(httplib::Response& res, int status, string body) {
    res.status = status;
    res.set_content(body, "application/json");
}

void sendError(httplib::Response& res, int status, string message) {
    sendJson(res, status, "{\"error\":" + jsonStr(message) + "}");
}

int main() {
    srand(time(0));
    loadAll();

    httplib::Server svr;

    svr.Post("/api/signup", [](const httplib::Request& req, httplib::Response& res) {
        lock_guard<mutex> lock(dataMutex);
        string username = req.get_param_value("username");
        string password = req.get_param_value("password");
        string contact = req.get_param_value("contact");

        if (username.empty() || password.empty() || contact.empty()) {
            sendError(res, 400, "Fill in every field.");
            return;
        }
        if (findUser(username) != -1) {
            sendError(res, 400, "That username is already taken.");
            return;
        }
        if (userCount >= 100) {
            sendError(res, 400, "User list is full.");
            return;
        }

        users[userCount] = new User(username, password, contact);
        userCount++;
        saveAll();
        sendJson(res, 200, "{\"ok\":true}");
    });

    svr.Post("/api/login", [](const httplib::Request& req, httplib::Response& res) {
        lock_guard<mutex> lock(dataMutex);
        string username = req.get_param_value("username");
        string password = req.get_param_value("password");

        int index = findUser(username);
        if (index == -1 || !users[index]->checkPassword(password)) {
            sendError(res, 401, "Wrong username or password.");
            return;
        }

        string token = makeToken();
        int slot = tokenCount % 200;
        tokens[slot] = token;
        tokenOwner[slot] = users[index];
        tokenCount++;

        sendJson(res, 200, "{\"token\":" + jsonStr(token) +
                           ",\"id\":" + to_string(users[index]->getUserId()) +
                           ",\"username\":" + jsonStr(users[index]->getUsername()) + "}");
    });

    svr.Post("/api/logout", [](const httplib::Request& req, httplib::Response& res) {
        lock_guard<mutex> lock(dataMutex);
        string token = req.get_header_value("X-Token");
        int n = tokenCount < 200 ? tokenCount : 200;
        for (int i = 0; i < n; i++) {
            if (!token.empty() && tokens[i] == token) {
                tokens[i] = "";
                tokenOwner[i] = nullptr;
            }
        }
        sendJson(res, 200, "{\"ok\":true}");
    });

    svr.Get("/api/me", [](const httplib::Request& req, httplib::Response& res) {
        lock_guard<mutex> lock(dataMutex);
        User* u = getCurrentUser(req);
        if (u == nullptr) {
            sendError(res, 401, "Please log in.");
            return;
        }
        sendJson(res, 200, "{\"id\":" + to_string(u->getUserId()) +
                           ",\"username\":" + jsonStr(u->getUsername()) + "}");
    });

    svr.Get("/api/items", [](const httplib::Request& req, httplib::Response& res) {
        lock_guard<mutex> lock(dataMutex);
        if (getCurrentUser(req) == nullptr) {
            sendError(res, 401, "Please log in.");
            return;
        }

        string type = req.get_param_value("type");
        string keyword = req.get_param_value("q");

        string out = "[";
        int shown = 0;
        int i = lostCount - 1;
        int j = foundCount - 1;

        while (i >= 0 || j >= 0) {
            Item* item;
            if (j < 0 || (i >= 0 && lostItems[i]->getId() > foundItems[j]->getId())) {
                item = lostItems[i];
                i--;
            }
            else {
                item = foundItems[j];
                j--;
            }

            if ((type == "lost" || type == "found") && item->getType() != type) {
                continue;
            }
            if (!keyword.empty() && !item->matches(keyword)) {
                continue;
            }

            User* reporter = findUserById(item->getReporterId());
            string reporterName = reporter ? reporter->getUsername() : "unknown";

            if (shown > 0) {
                out += ",";
            }
            out += item->toJson(reporterName);
            shown++;
        }

        out += "]";
        sendJson(res, 200, out);
    });

    svr.Post("/api/items", [](const httplib::Request& req, httplib::Response& res) {
        lock_guard<mutex> lock(dataMutex);
        User* u = getCurrentUser(req);
        if (u == nullptr) {
            sendError(res, 401, "Please log in.");
            return;
        }

        string type = req.get_param_value("type");
        string name = req.get_param_value("itemName");
        string description = req.get_param_value("description");
        string category = req.get_param_value("category");
        string place = req.get_param_value("place");
        string date = req.get_param_value("date");
        string extra = req.get_param_value("extra");

        if (name.empty() || description.empty() || place.empty() || date.empty() || extra.empty()) {
            sendError(res, 400, "Fill in every field.");
            return;
        }
        if (!validCategory(category)) {
            sendError(res, 400, "Choose a valid category.");
            return;
        }

        if (type == "lost") {
            if (lostCount >= 100) {
                sendError(res, 400, "Lost items list is full.");
                return;
            }
            lostItems[lostCount] = new LostItem(name, date, u->getUserId(), description,
                                                category, place, u->getContact(), extra);
            lostCount++;
        }
        else if (type == "found") {
            if (foundCount >= 100) {
                sendError(res, 400, "Found items list is full.");
                return;
            }
            foundItems[foundCount] = new FoundItem(name, date, u->getUserId(), description,
                                                   category, place, u->getContact(), extra);
            foundCount++;
        }
        else {
            sendError(res, 400, "Type must be lost or found.");
            return;
        }

        saveAll();
        sendJson(res, 200, "{\"ok\":true}");
    });

    svr.Post(R"(/api/items/(\d{1,9})/resolve)", [](const httplib::Request& req, httplib::Response& res) {
        lock_guard<mutex> lock(dataMutex);
        User* u = getCurrentUser(req);
        if (u == nullptr) {
            sendError(res, 401, "Please log in.");
            return;
        }

        int id = stoi(req.matches[1].str());

        for (int i = 0; i < lostCount; i++) {
            if (lostItems[i]->getId() == id) {
                if (lostItems[i]->getReporterId() != u->getUserId()) {
                    sendError(res, 403, "You can only resolve items you reported.");
                    return;
                }
                lostItems[i]->markResolved();
                saveAll();
                sendJson(res, 200, "{\"ok\":true}");
                return;
            }
        }

        for (int i = 0; i < foundCount; i++) {
            if (foundItems[i]->getId() == id) {
                if (foundItems[i]->getReporterId() != u->getUserId()) {
                    sendError(res, 403, "You can only resolve items you reported.");
                    return;
                }
                foundItems[i]->markResolved();
                saveAll();
                sendJson(res, 200, "{\"ok\":true}");
                return;
            }
        }

        sendError(res, 404, "No item with that ID.");
    });

    if (!svr.set_mount_point("/", "./www")) {
        cout << "Could not find the www folder. Put index.html inside a folder named www.\n";
        return 1;
    }

    cout << "Server running. Open http://127.0.0.1:8080 in your browser.\n";
    svr.listen("127.0.0.1", 8080);

    return 0;
}
