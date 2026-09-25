function main() -> void {
    user := json.parse("{\"name\":\"Nick\",\"age\":42,\"debug\":true,\"prefs\":{\"theme\":\"dark\"}}");
    public_user := user.omit(["debug"]);
    overrides := json.parse("{\"prefs\":{\"compact\":true}}");
    merged := public_user.merge_deep(overrides);
    print(json.stringify(merged));
    return;
}
