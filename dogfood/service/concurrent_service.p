async function answer(int value) -> int {
    return value + 1;
}
function main() -> void : ThreadError {
    mutex m;
    total := new(0);
    channel<int> jobs;
    producer := thread(() => {
        for (int i := 0; i < 100; i++) { jobs.send(i); }
        jobs.close();
    });
    consumer := thread(() => {
        for (int i := 0; i < 100; i++) {
            jobs.receive();
            m.lock(() => { *total = *total + 1; });
        }
    });
    producer.join();
    consumer.join();
    future := answer(*total);
    print(await future);
    return;
}
