function main() -> void : EmbedError {
    message := embed_file("assets/message.txt");
    files := embed_dir("assets");
    print(message);
    print(files["message.txt"]);
    return;
}
