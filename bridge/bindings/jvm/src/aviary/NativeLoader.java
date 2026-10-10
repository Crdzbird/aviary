package aviary;

import java.io.IOException;
import java.io.InputStream;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.StandardCopyOption;

/**
 * Resolves the aviary native library. Priority:
 *   1. -Daviary.path=... or $AVIARY_DYLIB (dev override)
 *   2. a bundled resource /native/&lt;os&gt;-&lt;arch&gt;/libaviary_c.&lt;ext&gt;
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

        String resource = "/native/" + osTag + "-" + archTag + "/libaviary_c." + ext;
        try (InputStream in = NativeLoader.class.getResourceAsStream(resource)) {
            if (in != null) {
                Path tmp = Files.createTempFile("libaviary_c", "." + ext);
                tmp.toFile().deleteOnExit();
                Files.copy(in, tmp, StandardCopyOption.REPLACE_EXISTING);
                return tmp;
            }
        } catch (IOException e) {
            throw new RuntimeException("Failed to extract bundled native library", e);
        }

        // Dev fallback: ../../../build/libaviary_c.dylib relative to this binding.
        Path dev = Path.of(System.getProperty("user.dir")).resolve("build/libaviary_c." + ext);
        if (Files.exists(dev)) return dev;
        throw new IllegalStateException("aviary native not found: set -Daviary.path=<lib>");
    }
}
