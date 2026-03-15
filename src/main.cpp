#include <algorithm>
#include <fstream>
#include <iostream>
#include <optional>
#include <string>
#include <vector>

struct Position {
    int row{};
    int col{};

    bool operator==(const Position& other) const {
        return row == other.row && col == other.col;
    }
};

enum class Direction { Up, Down, Left, Right };

struct Tank {
    Position pos{};
    Direction dir{Direction::Up};
    int hp{1};
};

struct Bullet {
    Position pos{};
    int dr{};
    int dc{};
    int reflections{};
    bool fromPlayer{false};
    bool alive{true};
};

class Game {
public:
    bool loadLevel(const std::string& mapPath, int levelIndex) {
        level_ = levelIndex;
        grid_.clear();
        enemies_.clear();
        bullets_.clear();
        player_ = Tank{};
        player_.hp = 3;
        player_.dir = Direction::Up;

        std::ifstream input(mapPath);
        if (!input) {
            std::cerr << "无法读取地图文件: " << mapPath << "\n";
            return false;
        }

        std::string line;
        while (std::getline(input, line)) {
            if (!line.empty() && line.back() == '\r') {
                line.pop_back();
            }
            if (!line.empty()) {
                grid_.push_back(line);
            }
        }

        if (grid_.empty()) {
            std::cerr << "地图为空: " << mapPath << "\n";
            return false;
        }

        const std::size_t width = grid_.front().size();
        for (const auto& row : grid_) {
            if (row.size() != width) {
                std::cerr << "地图宽度不一致: " << mapPath << "\n";
                return false;
            }
        }

        bool playerFound = false;
        for (int r = 0; r < static_cast<int>(grid_.size()); ++r) {
            for (int c = 0; c < static_cast<int>(grid_[r].size()); ++c) {
                if (grid_[r][c] == 'P') {
                    playerFound = true;
                    player_.pos = {r, c};
                    grid_[r][c] = '.';
                } else if (grid_[r][c] == 'E') {
                    Tank enemy;
                    enemy.pos = {r, c};
                    enemy.dir = Direction::Down;
                    enemy.hp = 1;
                    enemies_.push_back(enemy);
                    grid_[r][c] = '.';
                }
            }
        }

        if (!playerFound) {
            std::cerr << "地图缺少玩家出生点 P: " << mapPath << "\n";
            return false;
        }

        if (enemies_.empty()) {
            std::cerr << "地图缺少敌方坦克 E: " << mapPath << "\n";
            return false;
        }

        return true;
    }

    int enemyCount() const { return static_cast<int>(enemies_.size()); }

    bool runLevel() {
        while (true) {
            render();

            if (player_.hp <= 0) {
                std::cout << "你被击败了！\n";
                return false;
            }
            if (enemies_.empty()) {
                std::cout << "第 " << level_ << " 关通过！\n";
                return true;
            }

            std::cout << "输入操作（WASD移动，空格开火，q退出）: ";
            std::string command;
            if (!std::getline(std::cin, command)) {
                return false;
            }
            if (command == "q" || command == "Q") {
                return false;
            }

            char action = command.empty() ? ' ' : command[0];
            processPlayerAction(action);
            processEnemyAttacks();
            advanceBullets();
        }
    }

private:
    bool isInside(int r, int c) const {
        return r >= 0 && c >= 0 && r < static_cast<int>(grid_.size()) &&
               c < static_cast<int>(grid_[0].size());
    }

    bool isWall(int r, int c) const {
        if (!isInside(r, c)) {
            return true;
        }
        return grid_[r][c] == '#';
    }

    bool occupiedByEnemy(const Position& pos) const {
        return std::any_of(enemies_.begin(), enemies_.end(), [&](const Tank& t) {
            return t.hp > 0 && t.pos == pos;
        });
    }

    std::optional<int> enemyAt(const Position& pos) {
        for (int i = 0; i < static_cast<int>(enemies_.size()); ++i) {
            if (enemies_[i].hp > 0 && enemies_[i].pos == pos) {
                return i;
            }
        }
        return std::nullopt;
    }

    void render() const {
        std::vector<std::string> display = grid_;
        const int aliveEnemies = std::count_if(enemies_.begin(), enemies_.end(), [](const Tank& enemy) {
            return enemy.hp > 0;
        });

        for (const auto& enemy : enemies_) {
            if (enemy.hp > 0) {
                display[enemy.pos.row][enemy.pos.col] = 'E';
            }
        }

        for (const auto& bullet : bullets_) {
            if (bullet.alive && isInside(bullet.pos.row, bullet.pos.col)) {
                display[bullet.pos.row][bullet.pos.col] = bullet.fromPlayer ? '*' : 'o';
            }
        }

        if (player_.hp > 0) {
            display[player_.pos.row][player_.pos.col] = 'P';
        }

        std::cout << "\n=== 第 " << level_ << " 关 ===\n";
        std::cout << "玩家血量: " << player_.hp << "   敌方数量: " << aliveEnemies << "\n";
        std::cout << "图例: #墙体 .空地 P玩家 E敌人 *我方子弹 o敌方子弹\n";
        for (const auto& row : display) {
            std::cout << row << '\n';
        }
    }

    void processPlayerAction(char action) {
        switch (action) {
            case 'w':
            case 'W':
                player_.dir = Direction::Up;
                tryMovePlayer(-1, 0);
                break;
            case 's':
            case 'S':
                player_.dir = Direction::Down;
                tryMovePlayer(1, 0);
                break;
            case 'a':
            case 'A':
                player_.dir = Direction::Left;
                tryMovePlayer(0, -1);
                break;
            case 'd':
            case 'D':
                player_.dir = Direction::Right;
                tryMovePlayer(0, 1);
                break;
            case ' ':
                shootFromPlayer();
                break;
            default:
                break;
        }
    }

    void tryMovePlayer(int dr, int dc) {
        Position next{player_.pos.row + dr, player_.pos.col + dc};
        if (isWall(next.row, next.col) || occupiedByEnemy(next)) {
            return;
        }
        player_.pos = next;
    }

    void shootFromPlayer() {
        int dr = 0;
        int dc = 0;
        switch (player_.dir) {
            case Direction::Up:
                dr = -1;
                break;
            case Direction::Down:
                dr = 1;
                break;
            case Direction::Left:
                dc = -1;
                break;
            case Direction::Right:
                dc = 1;
                break;
        }

        Bullet bullet;
        bullet.pos = player_.pos;
        bullet.dr = dr;
        bullet.dc = dc;
        bullet.fromPlayer = true;
        bullet.reflections = 0;
        bullets_.push_back(bullet);
    }

    bool clearPathLine(const Position& from, const Position& to) const {
        if (from.row == to.row) {
            int start = std::min(from.col, to.col) + 1;
            int end = std::max(from.col, to.col);
            for (int c = start; c < end; ++c) {
                if (isWall(from.row, c)) {
                    return false;
                }
            }
            return true;
        }

        if (from.col == to.col) {
            int start = std::min(from.row, to.row) + 1;
            int end = std::max(from.row, to.row);
            for (int r = start; r < end; ++r) {
                if (isWall(r, from.col)) {
                    return false;
                }
            }
            return true;
        }

        return false;
    }

    void processEnemyAttacks() {
        for (const auto& enemy : enemies_) {
            if (enemy.hp <= 0) {
                continue;
            }

            if (enemy.pos.row == player_.pos.row && clearPathLine(enemy.pos, player_.pos)) {
                Bullet b;
                b.pos = enemy.pos;
                b.dr = 0;
                b.dc = (player_.pos.col > enemy.pos.col) ? 1 : -1;
                b.fromPlayer = false;
                bullets_.push_back(b);
            } else if (enemy.pos.col == player_.pos.col && clearPathLine(enemy.pos, player_.pos)) {
                Bullet b;
                b.pos = enemy.pos;
                b.dr = (player_.pos.row > enemy.pos.row) ? 1 : -1;
                b.dc = 0;
                b.fromPlayer = false;
                bullets_.push_back(b);
            }
        }
    }

    void applyReflection(Bullet& bullet) {
        if (bullet.reflections >= 3) {
            bullet.alive = false;
            return;
        }

        int nextR = bullet.pos.row + bullet.dr;
        int nextC = bullet.pos.col + bullet.dc;

        bool hitVerticalBoundary = !isInside(nextR, bullet.pos.col) || isWall(nextR, bullet.pos.col);
        bool hitHorizontalBoundary = !isInside(bullet.pos.row, nextC) || isWall(bullet.pos.row, nextC);

        if (hitVerticalBoundary) {
            bullet.dr = -bullet.dr;
        }
        if (hitHorizontalBoundary) {
            bullet.dc = -bullet.dc;
        }
        if (!hitVerticalBoundary && !hitHorizontalBoundary) {
            bullet.dr = -bullet.dr;
            bullet.dc = -bullet.dc;
        }

        bullet.reflections++;
    }

    void advanceBullets() {
        for (auto& bullet : bullets_) {
            if (!bullet.alive) {
                continue;
            }

            int nextR = bullet.pos.row + bullet.dr;
            int nextC = bullet.pos.col + bullet.dc;

            if (isWall(nextR, nextC)) {
                applyReflection(bullet);
                if (!bullet.alive) {
                    continue;
                }
                nextR = bullet.pos.row + bullet.dr;
                nextC = bullet.pos.col + bullet.dc;
                if (isWall(nextR, nextC)) {
                    bullet.alive = false;
                    continue;
                }
            }

            bullet.pos = {nextR, nextC};

            if (bullet.fromPlayer) {
                auto enemyIndex = enemyAt(bullet.pos);
                if (enemyIndex.has_value()) {
                    enemies_[*enemyIndex].hp = 0;
                    bullet.alive = false;
                }
            } else {
                if (bullet.pos == player_.pos && player_.hp > 0) {
                    player_.hp -= 1;
                    bullet.alive = false;
                }
            }
        }

        enemies_.erase(std::remove_if(enemies_.begin(), enemies_.end(), [](const Tank& e) { return e.hp <= 0; }), enemies_.end());
        bullets_.erase(std::remove_if(bullets_.begin(), bullets_.end(), [](const Bullet& b) { return !b.alive; }), bullets_.end());
    }

    int level_{1};
    std::vector<std::string> grid_;
    Tank player_{};
    std::vector<Tank> enemies_;
    std::vector<Bullet> bullets_;
};

int main() {
    std::vector<std::string> levels = {
        "maps/level1.txt",
        "maps/level2.txt",
        "maps/level3.txt",
    };

    std::vector<int> enemyCounts;
    enemyCounts.reserve(levels.size());

    for (int i = 0; i < static_cast<int>(levels.size()); ++i) {
        Game game;
        if (!game.loadLevel(levels[i], i + 1)) {
            return 1;
        }

        enemyCounts.push_back(game.enemyCount());
        if (i > 0 && enemyCounts[i] <= enemyCounts[i - 1]) {
            std::cerr << "难度校验失败：第 " << i + 1 << " 关敌方数量必须多于上一关。\n";
            return 1;
        }

        std::cout << "即将开始第 " << i + 1 << " 关（敌方数量: " << enemyCounts[i] << "）\n";
        if (!game.runLevel()) {
            std::cout << "游戏结束。\n";
            return 0;
        }
    }

    std::cout << "恭喜通关全部3关！\n";
    return 0;
}
