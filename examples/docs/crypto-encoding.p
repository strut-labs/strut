include <crypto>;
include <encoding>;
function main() -> int : (CryptoError, EncodingError) {
    bytes message := bytes.from_string("hello");
    bytes key := secure_random_bytes(32);
    bytes digest := sha256(message);
    bytes authenticator := hmac_sha256(key, message);
    string token := base64url_encode(authenticator);
    bytes decoded := base64url_decode(token);
    if (digest.length() != 32 || !constant_time_equal(authenticator, decoded)) {
        return 1;
    }
    return 0;
}
