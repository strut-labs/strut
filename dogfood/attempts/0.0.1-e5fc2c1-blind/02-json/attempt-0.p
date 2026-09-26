function main() -> int {
    user := json.parse("{\"name\":\"Ada\",\"active\":true,\"token\":\"secret\"}");
    public_user := user.omit(["token"]);
    print(json.stringify(public_user));
    return 0;
}
