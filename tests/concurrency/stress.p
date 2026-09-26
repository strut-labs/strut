async function plus_one(int x) -> int {
    return x + 1;
}
function main() -> void : ThreadError {
    mutex m;
    total := new(0);
    channel<int> jobs;
    producer := thread(() => {
        for (int i := 0; i < 1000; i++) {
            jobs.send(i);
        }
        jobs.close();
    });
    consumer := thread(() => {
        for (int i := 0; i < 1000; i++) {
            jobs.receive();
            m.lock(() => { *total = *total + 1; });
        }
    });
    producer.join();
    consumer.join();
    a := plus_one(40);
    b := plus_one(41);
    print(*total);
    print(await a + await b);
    return;
}
