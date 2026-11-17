#include <iostream>
#include <string>
#include <cctype>
#include <fstream>
using namespace std;

string toLower(string s) {
    for (int i = 0; i < s.length(); i++) {
        s[i] = tolower(s[i]);
    }
    return s;
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

    virtual void display() {
        cout << "ID: " << id;
        cout << "\nItem Name: " << name;
        cout << "\nDescription: " << description;
        cout << "\nCategory: " << category;
        cout << "\nLocation: " << location;
        cout << "\nDate: " << date;
        cout << "\nReporter ID: " << reporterID;
        cout << "\nResolved: " << (isResolved ? "Yes" : "No");
    }

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
    string getName(){
         return name; 
    }
    string getDescription(){
         return description; 
    }
    string getCategory(){
         return category; 
    }
    string getLocation(){
         return location; 
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

    void display() override {
        cout << "LOST ITEM\n";
        Item::display();
        cout << "\nOwner Contact: " << ownerContact;
        cout << "\nIdentifying Mark: " << identifyingMark;
        cout << "\n\n";
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

    void display() override {
        cout << "FOUND ITEM\n";
        Item::display();
        cout << "\nFinder Contact: " << finderContact;
        cout << "\nKept At: " << whereKept;
        cout << "\n\n";
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
User* currentUser = nullptr;

string categories[6] = {"Wallet", "Phone", "ID Card", "Bag", "Keys", "Other"};

string readLine(string ques) {
    string text;
    cout << ques;
    getline(cin, text);
    while (text.empty()) {
        cout << "This field cannot be empty. Try again: ";
        getline(cin, text);
    }
    return text;
}

int readInt(string ques) {
    int value;
    cout << ques;
    while (!(cin >> value)) {
        cin.clear();
        cin.ignore(1000, '\n');
        cout << "Invalid number. Try again: ";
    }
    cin.ignore(1000, '\n');
    return value;
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

string chooseCategory() {
    cout << "Category:\n";
    for (int i = 0; i < 6; i++) {
        cout << i + 1 << ". " << categories[i] << "\n";
    }
    int choice = readInt("Choose a category (1-6): ");
    if (choice < 1 || choice > 6) {
        cout << "Invalid choice, using Other.\n";
        return "Other";
    }
    return categories[choice - 1];
}

int findUser(string username) {
    for (int i = 0; i < userCount; i++) {
        if (users[i]->getUsername() == username) {
            return i;
        }
    }
    return -1;
}

void signUp() {
    if (userCount >= 100) {
        cout << "User list is full.\n";
        return;
    }

    cout << "\n--- Sign Up ---\n";
    string username = readLine("Choose a username: ");

    if (findUser(username) != -1) {
        cout << "That username is already taken.\n";
        return;
    }

    string password = readLine("Choose a password: ");
    string contact = readLine("Your contact number: ");

    users[userCount] = new User(username, password, contact);
    userCount++;
    saveAll();
    cout << "Account created. You can log in now.\n";
}

void login() {
    cout << "\n--- Login ---\n";
    string username = readLine("Username: ");
    string password = readLine("Password: ");

    int index = findUser(username);

    if (index == -1 || !users[index]->checkPassword(password)) {
        cout << "Wrong username or password.\n";
        return;
    }

    currentUser = users[index];
    cout << "Welcome, " << currentUser->getUsername() << "!\n";
}

void logout() {
    cout << "Goodbye, " << currentUser->getUsername() << ".\n";
    currentUser = nullptr;
}

void addLostItem() {
    if (lostCount >= 100) {
        cout << "Lost items list is full.\n";
        return;
    }

    cout << "\n--- Report a Lost Item ---\n";
    string name = readLine("Item name: ");
    string description = readLine("Description (color, brand, size, etc.): ");
    string category = chooseCategory();
    string location = readLine("Where did you lose it (Library, Hostel, Canteen...): ");
    string date = readLine("Date lost (YYYY-MM-DD): ");
    string mark = readLine("Identifying mark (sticker, scratch, etc.): ");

    lostItems[lostCount] = new LostItem(name, date, currentUser->getUserId(), description,
                                        category, location, currentUser->getContact(), mark);
    cout << "\nLost item reported. Its ID is " << lostItems[lostCount]->getId() << ".\n";
    lostCount++;
    saveAll();
}

void addFoundItem() {
    if (foundCount >= 100) {
        cout << "Found items list is full.\n";
        return;
    }

    cout << "\n--- Report a Found Item ---\n";
    string name = readLine("Item name: ");
    string description = readLine("Description (color, brand, size, etc.): ");
    string category = chooseCategory();
    string location = readLine("Where did you find it (Library, Hostel, Canteen...): ");
    string date = readLine("Date found (YYYY-MM-DD): ");
    string kept = readLine("Where is the item kept now: ");

    foundItems[foundCount] = new FoundItem(name, date, currentUser->getUserId(), description,
                                           category, location, currentUser->getContact(), kept);
    cout << "\nFound item reported. Its ID is " << foundItems[foundCount]->getId() << ".\n";
    foundCount++;
    saveAll();
}

void viewLostItems() {
    cout << "\n=== Lost Items (" << lostCount << ") ===\n";
    if (lostCount == 0) {
        cout << "No lost items reported yet.\n";
        return;
    }
    for (int i = 0; i < lostCount; i++) {
        lostItems[i]->display();
        cout << "\n";
    }
}

void viewFoundItems() {
    cout << "\n=== Found Items (" << foundCount << ") ===\n";
    if (foundCount == 0) {
        cout << "No found items reported yet.\n";
        return;
    }
    for (int i = 0; i < foundCount; i++) {
        foundItems[i]->display();
        cout << "\n";
    }
}

void searchItems() {
    cout << "\n--- Search Items ---\n";
    string keyword = readLine("Enter keyword: ");

    int total = 0;

    for (int i = 0; i < lostCount; i++) {
        if (lostItems[i]->matches(keyword)) {
            lostItems[i]->display();
            cout << "\n";
            total++;
        }
    }

    for (int i = 0; i < foundCount; i++) {
        if (foundItems[i]->matches(keyword)) {
            foundItems[i]->display();
            cout << "\n";
            total++;
        }
    }

    if (total == 0) {
        cout << "No items matched \"" << keyword << "\".\n";
    }
    else {
        cout << total << " item(s) matched.\n";
    }
}

void resolveItem() {
    cout << "\n--- Mark an Item as Resolved ---\n";
    int id = readInt("Enter item ID: ");

    for (int i = 0; i < lostCount; i++) {
        if (lostItems[i]->getId() == id) {
            if (lostItems[i]->getReporterId() != currentUser->getUserId()) {
                cout << "You can only resolve items you reported.\n";
                return;
            }
            lostItems[i]->markResolved();
            cout << "Lost item " << id << " marked as resolved.\n";
            saveAll();
            return;
        }
    }

    for (int i = 0; i < foundCount; i++) {
        if (foundItems[i]->getId() == id) {
            if (foundItems[i]->getReporterId() != currentUser->getUserId()) {
                cout << "You can only resolve items you reported.\n";
                return;
            }
            foundItems[i]->markResolved();
            cout << "Found item " << id << " marked as resolved.\n";
            saveAll();
            return;
        }
    }

    cout << "No item with ID " << id << " was found.\n";
}

void showStartMenu() {
    cout << "\n=============================\n";
    cout << "   SMART LOST & FOUND SYSTEM\n";
    cout << "=============================\n";
    cout << "1. Sign up\n";
    cout << "2. Login\n";
    cout << "3. Exit\n";
    cout << "-----------------------------\n";
}

void showMenu() {
    cout << "\n=============================\n";
    cout << "   SMART LOST & FOUND SYSTEM\n";
    cout << "=============================\n";
    cout << "1. Report a lost item\n";
    cout << "2. Report a found item\n";
    cout << "3. View all lost items\n";
    cout << "4. View all found items\n";
    cout << "5. Search by keyword\n";
    cout << "6. Mark an item as resolved\n";
    cout << "7. Logout\n";
    cout << "-----------------------------\n";
}

int main() {
    bool running = true;

    loadAll();

    while (running) {
        if (currentUser == nullptr) {
            showStartMenu();
            int choice = readInt("Enter your choice: ");

            if (choice == 1) {
                signUp();
            }
            else if (choice == 2) {
                login();
            }
            else if (choice == 3) {
                cout << "\nGoodbye!\n";
                running = false;
            }
            else {
                cout << "\nInvalid choice. Please enter 1-3.\n";
            }
        }
        else {
            showMenu();
            int choice = readInt("Enter your choice: ");

            if (choice == 1) {
                addLostItem();
            }
            else if (choice == 2) {
                addFoundItem();
            }
            else if (choice == 3) {
                viewLostItems();
            }
            else if (choice == 4) {
                viewFoundItems();
            }
            else if (choice == 5) {
                searchItems();
            }
            else if (choice == 6) {
                resolveItem();
            }
            else if (choice == 7) {
                logout();
            }
            else {
                cout << "\nInvalid choice. Please enter 1-7.\n";
            }
        }
    }

    for (int i = 0; i < lostCount; i++) delete lostItems[i];
    for (int i = 0; i < foundCount; i++) delete foundItems[i];
    for (int i = 0; i < userCount; i++) delete users[i];

    return 0;
}