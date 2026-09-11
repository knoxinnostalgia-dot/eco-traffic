#include <SFML/Graphics.hpp>
#include <vector>
#include <iostream>
#include <iomanip>
#include <sstream>
#include <random>
#include <algorithm>
#include <string>

// Physics Constants
const float CO2_IDLE = 1.0f;    // g/s while stopped
const float CO2_CRUISE = 0.4f;  // g/s while moving at constant speed
const float CO2_ACCEL = 2.5f;   // g/s while accelerating
const float CO2_DECEL = 0.9f;   // g/s while slowing for orange

const float WINDOW_W = 1100.0f;
const float WINDOW_H = 720.0f;
const float HUD_BOTTOM = 188.0f;
const float ROAD_Y = 400.0f;
const float ROAD_H = 110.0f;
const float ROAD_CENTER_Y = ROAD_Y + ROAD_H * 0.5f;

enum LightState { GREEN, ORANGE, RED };
enum ControlMode { AUTOMATIC, MANUAL };

std::mt19937& rng() {
    static std::mt19937 engine{std::random_device{}()};
    return engine;
}

float randomRange(float minValue, float maxValue) {
    std::uniform_real_distribution<float> dist(minValue, maxValue);
    return dist(rng());
}

LightState nextSignal(LightState light) {
    if (light == GREEN) return ORANGE;
    if (light == ORANGE) return RED;
    return GREEN;
}

float phaseDuration(LightState light) {
    if (light == GREEN) return 5.0f;
    if (light == ORANGE) return 2.4f;
    return 5.0f;
}

const char* signalLabel(LightState light) {
    if (light == GREEN) return "GO (GREEN)";
    if (light == ORANGE) return "SLOW (ORANGE)";
    return "STOP (RED)";
}

class Vehicle {
public:
    sf::RectangleShape shape;
    sf::RectangleShape window;
    float x, y;
    float speed;
    float maxSpeed;
    float acceleration;
    float co2Emitted;
    bool isIdling;
    bool isAccelerating;
    bool isDecelerating;

    Vehicle(float startX, float startY, float randomMaxSpeed) {
        x = startX;
        y = startY;
        speed = 0.0f;
        maxSpeed = randomMaxSpeed;
        acceleration = randomRange(0.10f, 0.20f);
        co2Emitted = 0.0f;
        isIdling = false;
        isAccelerating = false;
        isDecelerating = false;

        shape.setSize(sf::Vector2f(36.0f, 18.0f));
        shape.setFillColor(sf::Color(46, 204, 113));
        shape.setOutlineThickness(1.0f);
        shape.setOutlineColor(sf::Color(15, 23, 42));
        shape.setOrigin(18.0f, 9.0f);
        shape.setPosition(x, y);

        window.setSize(sf::Vector2f(12.0f, 10.0f));
        window.setFillColor(sf::Color(186, 230, 253));
        window.setOrigin(0.0f, 5.0f);
        window.setPosition(x + 2.0f, y);
    }

    // Orange-light kinematics: ease off the throttle instead of a full stop.
    void decelerate() {
        isDecelerating = true;
        isAccelerating = false;
        isIdling = false;
        speed -= acceleration * 1.15f;
        const float cautionSpeed = std::max(0.55f, maxSpeed * 0.22f);
        if (speed < cautionSpeed) {
            speed = cautionSpeed;
        }
    }

    void update(LightState light, float stopLineX, float dt, float carAheadX) {
        isDecelerating = false;
        const bool beforeLine = x < stopLineX;
        const bool shouldStop = (light == RED && beforeLine && x + 40.0f >= stopLineX);
        // Only cars already in the approach zone ease off; distant cars stay at cruise/accel.
        const bool shouldSlowForOrange = (light == ORANGE && beforeLine && x + 140.0f >= stopLineX);

        if (shouldStop) {
            if (speed > 0.0f) {
                speed -= acceleration * 1.5f;
                if (speed < 0.0f) speed = 0.0f;
                isAccelerating = false;
                isIdling = false;
            } else {
                isIdling = true;
                isAccelerating = false;
            }
        } else if (shouldSlowForOrange) {
            decelerate();
        } else {
            isIdling = false;
            if (speed < maxSpeed) {
                speed += acceleration;
                isAccelerating = true;
            } else {
                speed = maxSpeed;
                isAccelerating = false;
            }
        }

        const float minGap = 48.0f;
        if (carAheadX < 1.0e8f && (carAheadX - x) < minGap) {
            speed = std::min(speed, std::max(0.0f, (carAheadX - x - 38.0f) * 0.2f));
            if (speed < 0.08f) {
                speed = 0.0f;
                isIdling = true;
                isAccelerating = false;
                isDecelerating = false;
            }
        }

        x += speed;
        shape.setPosition(x, y);
        window.setPosition(x + 2.0f, y);

        if (isIdling) {
            co2Emitted += CO2_IDLE * dt;
            shape.setFillColor(sf::Color(250, 204, 21));
        } else if (isDecelerating) {
            co2Emitted += CO2_DECEL * dt;
            shape.setFillColor(sf::Color(249, 115, 22));
        } else if (isAccelerating) {
            co2Emitted += CO2_ACCEL * dt;
            shape.setFillColor(sf::Color(239, 68, 68));
        } else {
            co2Emitted += CO2_CRUISE * dt;
            shape.setFillColor(sf::Color(56, 189, 248));
        }
    }
};

int main() {
    sf::RenderWindow window(sf::VideoMode(static_cast<unsigned int>(WINDOW_W),
                                          static_cast<unsigned int>(WINDOW_H)),
                            "Eco-Traffic Framework Engine");
    window.setFramerateLimit(60);

    sf::RectangleShape sky(sf::Vector2f(WINDOW_W, WINDOW_H));
    sky.setFillColor(sf::Color(15, 23, 42));

    sf::RectangleShape hudPanel(sf::Vector2f(WINDOW_W - 32.0f, 156.0f));
    hudPanel.setPosition(16.0f, 16.0f);
    hudPanel.setFillColor(sf::Color(30, 41, 59, 235));
    hudPanel.setOutlineThickness(1.5f);
    hudPanel.setOutlineColor(sf::Color(71, 85, 105));

    sf::RectangleShape ground(sf::Vector2f(WINDOW_W, WINDOW_H - HUD_BOTTOM));
    ground.setPosition(0.0f, HUD_BOTTOM);
    ground.setFillColor(sf::Color(21, 32, 43));

    sf::RectangleShape shoulder(sf::Vector2f(WINDOW_W, 28.0f));
    shoulder.setPosition(0.0f, ROAD_Y - 28.0f);
    shoulder.setFillColor(sf::Color(22, 101, 52));

    sf::RectangleShape lowerShoulder(sf::Vector2f(WINDOW_W, 36.0f));
    lowerShoulder.setPosition(0.0f, ROAD_Y + ROAD_H);
    lowerShoulder.setFillColor(sf::Color(22, 101, 52));

    sf::RectangleShape road(sf::Vector2f(WINDOW_W, ROAD_H));
    road.setFillColor(sf::Color(30, 41, 59));
    road.setPosition(0.0f, ROAD_Y);

    std::vector<sf::RectangleShape> laneDashes;
    for (float dashX = 20.0f; dashX < WINDOW_W; dashX += 52.0f) {
        sf::RectangleShape dash(sf::Vector2f(28.0f, 4.0f));
        dash.setFillColor(sf::Color(250, 204, 21, 180));
        dash.setPosition(dashX, ROAD_Y + ROAD_H * 0.5f - 2.0f);
        laneDashes.push_back(dash);
    }

    const float stopLineX = 620.0f;
    sf::RectangleShape stopLine(sf::Vector2f(6.0f, ROAD_H - 12.0f));
    stopLine.setFillColor(sf::Color(248, 250, 252));
    stopLine.setPosition(stopLineX, ROAD_Y + 6.0f);

    sf::RectangleShape lightHousing(sf::Vector2f(46.0f, 122.0f));
    lightHousing.setFillColor(sf::Color(15, 23, 42));
    lightHousing.setOutlineThickness(3.0f);
    lightHousing.setOutlineColor(sf::Color(51, 65, 85));
    lightHousing.setPosition(stopLineX + 28.0f, ROAD_Y - 150.0f);

    sf::RectangleShape lightPole(sf::Vector2f(8.0f, 132.0f));
    lightPole.setFillColor(sf::Color(71, 85, 105));
    lightPole.setPosition(stopLineX + 47.0f, ROAD_Y - 32.0f);

    sf::CircleShape bulbRed(14.0f);
    sf::CircleShape bulbOrange(14.0f);
    sf::CircleShape bulbGreen(14.0f);
    bulbRed.setPosition(stopLineX + 37.0f, ROAD_Y - 142.0f);
    bulbOrange.setPosition(stopLineX + 37.0f, ROAD_Y - 104.0f);
    bulbGreen.setPosition(stopLineX + 37.0f, ROAD_Y - 66.0f);

    sf::RectangleShape legendBar(sf::Vector2f(WINDOW_W - 32.0f, 52.0f));
    legendBar.setPosition(16.0f, WINDOW_H - 68.0f);
    legendBar.setFillColor(sf::Color(30, 41, 59, 230));
    legendBar.setOutlineThickness(1.0f);
    legendBar.setOutlineColor(sf::Color(71, 85, 105));

    LightState currentLight = RED;
    ControlMode currentMode = AUTOMATIC;
    std::vector<Vehicle> trafficQueue;

    sf::Clock spawnClock;
    sf::Clock systemClock;
    float totalSystemCO2 = 0.0f;
    float autoLightTimer = 0.0f;
    float manualOverrideTimer = 0.0f;
    float nextSpawnDelay = randomRange(1.1f, 3.2f);
    const float OVERRIDE_TIMEOUT = 8.0f;

    sf::Font font;
    if (!font.loadFromFile("C:\\Windows\\Fonts\\arial.ttf") &&
        !font.loadFromFile("C:\\Windows\\Fonts\\segoeui.ttf")) {
        std::cout << "Warning: Could not load a HUD font.\n";
    }

    sf::Text titleText;
    titleText.setFont(font);
    titleText.setCharacterSize(22);
    titleText.setStyle(sf::Text::Bold);
    titleText.setFillColor(sf::Color(226, 232, 240));
    titleText.setPosition(36.0f, 28.0f);
    titleText.setString("ECO-TRAFFIC  •  SIGNAL KINEMATICS LAB");

    sf::Text hudText;
    hudText.setFont(font);
    hudText.setCharacterSize(16);
    hudText.setFillColor(sf::Color(203, 213, 225));
    hudText.setPosition(36.0f, 62.0f);

    sf::Text legendText;
    legendText.setFont(font);
    legendText.setCharacterSize(15);
    legendText.setFillColor(sf::Color(226, 232, 240));
    legendText.setPosition(36.0f, WINDOW_H - 52.0f);
    legendText.setString(
        "Cars:  Red = accelerating   Orange = decelerating (amber)   Yellow = idling   Blue = cruising     |     SPACE cycles Red / Orange / Green");

    while (window.isOpen()) {
        sf::Event event;
        while (window.pollEvent(event)) {
            if (event.type == sf::Event::Closed) {
                window.close();
            }
            if (event.type == sf::Event::KeyPressed && event.key.code == sf::Keyboard::Space) {
                currentMode = MANUAL;
                currentLight = nextSignal(currentLight);
                manualOverrideTimer = 0.0f;
            }
        }

        float dt = systemClock.restart().asSeconds();

        if (currentMode == AUTOMATIC) {
            autoLightTimer += dt;
            if (autoLightTimer >= phaseDuration(currentLight)) {
                currentLight = nextSignal(currentLight);
                autoLightTimer = 0.0f;
            }
        } else {
            manualOverrideTimer += dt;
            if (manualOverrideTimer >= OVERRIDE_TIMEOUT) {
                currentMode = AUTOMATIC;
                autoLightTimer = 0.0f;
            }
        }

        if (spawnClock.getElapsedTime().asSeconds() > nextSpawnDelay) {
            const bool laneClear = trafficQueue.empty() || trafficQueue.back().x > 70.0f;
            if (laneClear) {
                const float randomMaxSpeed = randomRange(2.4f, 5.2f);
                trafficQueue.push_back(Vehicle(0.0f, ROAD_CENTER_Y, randomMaxSpeed));
                nextSpawnDelay = randomRange(1.1f, 3.4f);
                spawnClock.restart();
            }
        }

        const sf::Color redOn(239, 68, 68);
        const sf::Color orangeOn(249, 115, 22);
        const sf::Color greenOn(34, 197, 94);
        const sf::Color redOff(90, 24, 24);
        const sf::Color orangeOff(92, 42, 12);
        const sf::Color greenOff(20, 64, 32);
        bulbRed.setFillColor(currentLight == RED ? redOn : redOff);
        bulbOrange.setFillColor(currentLight == ORANGE ? orangeOn : orangeOff);
        bulbGreen.setFillColor(currentLight == GREEN ? greenOn : greenOff);

        totalSystemCO2 = 0.0f;
        for (auto it = trafficQueue.begin(); it != trafficQueue.end();) {
            float carAheadX = 1.0e9f;
            for (const auto& other : trafficQueue) {
                if (other.x > it->x) {
                    carAheadX = std::min(carAheadX, other.x);
                }
            }
            it->update(currentLight, stopLineX, dt, carAheadX);
            totalSystemCO2 += it->co2Emitted;

            if (it->x > WINDOW_W + 50.0f) {
                it = trafficQueue.erase(it);
            } else {
                ++it;
            }
        }

        std::stringstream ss;
        ss << "Mode  " << (currentMode == AUTOMATIC ? "AUTOMATIC TIMER" : "MANUAL OVERRIDE")
           << "     Space : cycle signal     "
           << (currentMode == MANUAL
                   ? "Returns to auto in " + (std::ostringstream() << std::fixed << std::setprecision(1)
                                                                 << (OVERRIDE_TIMEOUT - manualOverrideTimer) << "s").str()
                   : "Next phase in " + (std::ostringstream() << std::fixed << std::setprecision(1)
                                                              << (phaseDuration(currentLight) - autoLightTimer) << "s").str())
           << "\n"
           << "Signal  " << signalLabel(currentLight)
           << "     Active cars  " << trafficQueue.size()
           << "     Spawn gap  " << std::fixed << std::setprecision(1) << nextSpawnDelay << "s (random)"
           << "\n"
           << "CO2 mass  " << std::fixed << std::setprecision(2) << totalSystemCO2 << " g"
           << "     Green = go    Orange = near cars slow    Red = stop"
           << "     HUD stays above the road so cars never sit under the text";
        hudText.setString(ss.str());

        window.clear();
        window.draw(sky);
        window.draw(ground);
        window.draw(shoulder);
        window.draw(lowerShoulder);
        window.draw(road);
        for (const auto& dash : laneDashes) {
            window.draw(dash);
        }
        window.draw(stopLine);
        window.draw(lightPole);
        window.draw(lightHousing);
        window.draw(bulbRed);
        window.draw(bulbOrange);
        window.draw(bulbGreen);

        for (const auto& vehicle : trafficQueue) {
            window.draw(vehicle.shape);
            window.draw(vehicle.window);
        }

        window.draw(hudPanel);
        window.draw(titleText);
        window.draw(hudText);
        window.draw(legendBar);
        window.draw(legendText);
        window.display();
    }

    return 0;
}
