<?php
// get.php - Doc du lieu tu bang `dulieu` (database pbl3_iot) va hien thi len web
// Cach dung:
//   get.php            -> trang web (tu dong lam moi moi 10 giay)
//   get.php?limit=100  -> so dong hien thi (mac dinh 50, toi da 500)
//   get.php?json=1     -> tra ve JSON thay vi HTML

date_default_timezone_set('Asia/Ho_Chi_Minh');

// ===== Cau hinh database =====
$host    = '127.0.0.1';
$db      = 'pbl3_iot';
$user    = 'root';
$pass    = '';
$charset = 'utf8mb4';
$table   = 'dulieu';

$limit = isset($_GET['limit']) ? (int)$_GET['limit'] : 50;
$limit = max(1, min(500, $limit));

$rows  = [];
$error = null;

try {
    $pdo = new PDO("mysql:host=$host;dbname=$db;charset=$charset", $user, $pass, [
        PDO::ATTR_ERRMODE            => PDO::ERRMODE_EXCEPTION,
        PDO::ATTR_DEFAULT_FETCH_MODE => PDO::FETCH_ASSOC,
    ]);

    // ORDER BY 1 = sap xep theo cot dau tien (thuong la id), moi nhat truoc
    $rows = $pdo->query("SELECT * FROM `$table` ORDER BY 1 DESC LIMIT $limit")->fetchAll();
} catch (PDOException $e) {
    http_response_code(500);
    $error = $e->getMessage();
}

// ===== Che do JSON =====
if (isset($_GET['json'])) {
    header("Access-Control-Allow-Origin: *");
    header("Content-Type: application/json; charset=utf-8");
    echo json_encode(
        $error ? ["status" => "error", "message" => $error] : ["status" => "ok", "count" => count($rows), "data" => $rows],
        JSON_UNESCAPED_UNICODE
    );
    exit;
}

// ===== Che do HTML =====
header("Content-Type: text/html; charset=utf-8");
function h($v) { return htmlspecialchars((string)$v, ENT_QUOTES, 'UTF-8'); }

$latest  = $rows[0] ?? null;
$columns = $rows ? array_keys($rows[0]) : [];
?>
<!DOCTYPE html>
<html lang="vi">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<meta http-equiv="refresh" content="10">
<title>PBL3 - Du lieu cam bien</title>
<style>
  body { font-family: system-ui, sans-serif; margin: 0; padding: 20px; background: #f4f6f8; color: #222; }
  h1 { font-size: 1.4rem; margin: 0 0 4px; }
  .sub { color: #666; font-size: .85rem; margin-bottom: 16px; }
  .cards { display: flex; flex-wrap: wrap; gap: 12px; margin-bottom: 20px; }
  .card { background: #fff; border-radius: 10px; padding: 14px 18px; min-width: 130px; box-shadow: 0 1px 3px rgba(0,0,0,.1); }
  .card .k { font-size: .8rem; color: #666; text-transform: uppercase; }
  .card .v { font-size: 1.6rem; font-weight: 600; }
  .wrap { overflow-x: auto; background: #fff; border-radius: 10px; box-shadow: 0 1px 3px rgba(0,0,0,.1); }
  table { border-collapse: collapse; width: 100%; }
  th, td { padding: 8px 14px; text-align: left; border-bottom: 1px solid #eee; white-space: nowrap; }
  th { background: #fafafa; font-size: .8rem; text-transform: uppercase; color: #555; }
  .err { background: #fdecea; color: #b71c1c; padding: 12px; border-radius: 8px; }
</style>
</head>
<body>
  <h1>Du lieu cam bien PBL3</h1>
  <div class="sub">Cap nhat luc <?= date('H:i:s d/m/Y') ?> &middot; tu dong lam moi moi 10 giay &middot; <a href="?json=1&limit=<?= $limit ?>">JSON</a></div>

<?php if ($error): ?>
  <div class="err">Loi database: <?= h($error) ?></div>
<?php elseif (!$rows): ?>
  <div class="err">Bang <?= h($table) ?> chua co du lieu.</div>
<?php else: ?>
  <div class="cards">
    <?php foreach ($latest as $k => $v): ?>
      <div class="card"><div class="k"><?= h($k) ?></div><div class="v"><?= h($v) ?></div></div>
    <?php endforeach; ?>
  </div>

  <div class="wrap">
    <table>
      <thead><tr><?php foreach ($columns as $c): ?><th><?= h($c) ?></th><?php endforeach; ?></tr></thead>
      <tbody>
      <?php foreach ($rows as $r): ?>
        <tr><?php foreach ($r as $v): ?><td><?= h($v) ?></td><?php endforeach; ?></tr>
      <?php endforeach; ?>
      </tbody>
    </table>
  </div>
<?php endif; ?>
</body>
</html>