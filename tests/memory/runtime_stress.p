function main() -> void : ThreadError {
    owner := ptr(7);
    weak_owner := weak(owner);
    worker := thread(() => {
        for (int i := 0; i < 10000; i++) {
            locked := weak_owner.lock();
            if (locked != null) {
                value := *locked;
            }
        }
    });
    owner = null;
    worker.join();
    print(weak_owner.expired());
    return;
}
