package aviary;

import java.io.IOException;
import java.io.InputStream;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.StandardCopyOption;

/**
 * Resolves the aviary native library. Priority:
 *   1. -Daviary.path=... or $AVIARY_DYLIB (dev override)
 *   2. the bundled resources /native/&lt;os&gt;-&lt;arch&gt;/ (index.txt lists every library)
 *   3. the repo build/ dir (dev fallback)
 *
 * The aviary library links cryptolib_c; on a dev build its embedded rpath
 * resolves libcryptolib_c automatically. A bundled JAR should ship both libs.
 */
public final class NativeLoader {
    private NativeLoader() {}

    public static Path resolve() {
        String os = System.getProperty("os.name", "").toLowerCase();
        String arch = System.getProperty("os.arch", "").toLowerCase();
        String osTag = os.contains("mac") ? "darwin" : os.contains("win") ? "windows" : "linux";
        String archTag = (arch.contains("aarch64") || arch.contains("arm64")) ? "arm64" : "x64";
        String ext = osTag.equals("darwin") ? "dylib" : osTag.equals("windows") ? "dll" : "so";

        String override = System.getProperty("aviary.path", System.getenv("AVIARY_DYLIB"));
        if (override != null && !override.isEmpty()) return Path.of(override);

        String dir = "/native/" + osTag + "-" + archTag + "/";
        String main = "libaviary_c." + ext;
        // A bundled jar lists every library of the slot in index.txt so the
        // whole set lands in one directory: libaviary_c finds its CipherBird
        // sibling through its loader relative install name.
        try (InputStream index = NativeLoader.class.getResourceAsStream(dir + "index.txt")) {
            if (index != null) {
                Path tmpDir = Files.createTempDirectory("aviary-native");
                tmpDir.toFile().deleteOnExit();
                for (String name : new String(index.readAllBytes(), java.nio.charset.StandardCharsets.UTF_8).split("\\R")) {
                    if (name.isBlank()) continue;
                    try (InputStream in = NativeLoader.class.getResourceAsStream(dir + name)) {
                        if (in == null) throw new IllegalStateException("bundled native listed but missing: " + name);
                        Path out = tmpDir.resolve(name);
                        Files.copy(in, out, StandardCopyOption.REPLACE_EXISTING);
                        out.toFile().deleteOnExit();
                    }
                }
                return tmpDir.resolve(main);
            }
        } catch (IOException e) {
            throw new RuntimeException("Failed to extract bundled native libraries", e);
        }
        // Dev fallback: ../../../build/libaviary_c.dylib relative to this binding.
        Path dev = Path.of(System.getProperty("user.dir")).resolve("build/libaviary_c." + ext);
        if (Files.exists(dev)) return dev;
        throw new IllegalStateException("aviary native not found: set -Daviary.path=<lib>");
    }
}
