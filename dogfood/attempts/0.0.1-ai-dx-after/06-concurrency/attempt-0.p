function main() -> int : ThreadError {
    channel<int> jobs;
    producer := thread(() => { jobs.send(42); jobs.close(); });
    value := jobs.receive();
    print(value ?? 0);
    producer.join();
    return 0;
}
