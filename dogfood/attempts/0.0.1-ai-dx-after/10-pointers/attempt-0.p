struct User { string name; }
function main() -> int {
    User* user := new(User { name: "Ada" });
    unsafe {
        raw_ptr<User> raw := ptr(user);
        print(raw->name);
    }
    return 0;
}
