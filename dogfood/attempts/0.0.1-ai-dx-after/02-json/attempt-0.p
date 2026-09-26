function main() -> int {
    user := json.parse("{\"name\":\"Ada\",\"active\":true,\"token\":\"secret\"}");
    print(json.stringify(user.omit(["token"])));
    return 0;
}
