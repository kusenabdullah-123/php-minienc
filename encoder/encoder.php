<?php
// ==========================================
// PHP MINIENC FRAMEWORK BUILDER/ENCODER
// ==========================================

// --- KONFIGURASI KRITIS ---
// Kunci ini harus SAMA PERSIS dengan #define SERVER_KEY_PART di minienc.c
const SERVER_KEY_PART = 'KunciServerRahasia'; 
const MY_SIGNATURE    = 'MINIENC';

// Folder Sumber dan Tujuan
$sourceDir = __DIR__ . '/source_code'; 
$targetDir = __DIR__ . '/build_output'; 

// Pengaturan Pengecualian
$ignoredItems = ['.git', '.idea', '.vscode', 'tests', 'builder.php', 'README.md', '.DS_Store', 'vendor', 'Migrations', 'node_modules'];
$skipEncryptionExt = ['html', 'css', 'js', 'json', 'xml', 'jpg', 'png', 'env', 'stub', 'txt'];
$skipEncryptionFiles = ['config.php', 'database.php', 'routes.php', 'constants.php'];

// ==========================================
// FUNGSI ENKRIPSI
// ==========================================

function encryptFile($input, $output) {
    $code = file_get_contents($input);
    
    // Generate Key & IV Unik
    $fileKeyPart = openssl_random_pseudo_bytes(16);
    $iv = openssl_random_pseudo_bytes(16);
    $fullKey = $fileKeyPart . SERVER_KEY_PART;
    
    // Enkripsi AES-256-CBC
    $encrypted = openssl_encrypt($code, 'AES-256-CBC', $fullKey, OPENSSL_RAW_DATA, $iv);
    
    if ($encrypted === false) {
        echo "[ERROR] Gagal enkripsi $input\n";
        return;
    }
    
    // Gabung Struktur File: [SIGNATURE] [KEY_PART] [IV] [DATA]
    $content = MY_SIGNATURE . $fileKeyPart . $iv . $encrypted;
    file_put_contents($output, $content);
}

// ==========================================
// FUNGSI UTAMA (REKURSIF)
// ==========================================

function processDirectory($src, $dst) {
    global $ignoredItems, $skipEncryptionExt, $skipEncryptionFiles;

    if (!is_dir($dst)) {
        mkdir($dst, 0777, true);
    }

    $dir = opendir($src);
    while (($file = readdir($dir)) !== false) {
        if ($file == '.' || $file == '..') continue;
        if (in_array($file, $ignoredItems)) continue;

        $srcPath = $src . '/' . $file;
        $dstPath = $dst . '/' . $file;

        if (is_dir($srcPath)) {
            // Masuk ke subfolder
            processDirectory($srcPath, $dstPath);
        } else {
            $ext = pathinfo($file, PATHINFO_EXTENSION);
            
            $isPhp = ($ext === 'php');
            $isSkippedExt = in_array($ext, $skipEncryptionExt);
            $isSkippedFile = in_array($file, $skipEncryptionFiles);

            if ($isPhp && !$isSkippedExt && !$isSkippedFile) {
                // ENKRIPSI LOGIC
                echo "🔒 Encrypting: " . str_replace(__DIR__, '', $srcPath) . "\n";
                encryptFile($srcPath, $dstPath);
            } else {
                // COPY BIASA (File Config/Asset)
                echo "📄 Copying   : " . str_replace(__DIR__, '', $srcPath) . "\n";
                copy($srcPath, $dstPath);
            }
        }
    }
    closedir($dir);
}

// --- EKSEKUSI ---
echo "========================================\n";
echo "       PHP MiniEnc BUILDER v0.1         \n";
echo "========================================\n";
echo "Source Dir: $sourceDir\n";
echo "Target Dir: $targetDir\n";
echo "----------------------------------------\n";

if (!is_dir($sourceDir)) {
    die("Error: Folder source '$sourceDir' tidak ditemukan. Pastikan folder framework Anda ada!\n");
}

// Hapus folder tujuan lama sebelum build
if (is_dir($targetDir)) {
    echo "Menghapus folder target lama...\n";
    array_map('unlink', glob("$targetDir/*"));
    rmdir($targetDir);
}

processDirectory($sourceDir, $targetDir);

echo "\n✅ SELESAI! Folder '$targetDir' siap didistribusikan.\n";
?>