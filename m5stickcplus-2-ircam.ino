#include <Wire.h>
#include <Adafruit_AMG88xx.h>
#include <M5StickCPlus2.h>

Adafruit_AMG88xx amg;

#define SCREEN_WIDTH 240
#define SCREEN_HEIGHT 135
#define GRID_SIZE 8
#define INTERPOLATED_SIZE 16  // Reduce for faster refresh

float lastDisplayed[INTERPOLATED_SIZE][INTERPOLATED_SIZE];

float interpolate(float start, float end, float fraction) {
    return start + (end - start) * fraction;
}

void interpolateImage(float input[GRID_SIZE][GRID_SIZE], float output[INTERPOLATED_SIZE][INTERPOLATED_SIZE]) {
    for (int i = 0; i < INTERPOLATED_SIZE; i++) {
        float rowRatio = (float)(i) / (INTERPOLATED_SIZE - 1) * (GRID_SIZE - 1);
        int iLow = floor(rowRatio);
        int iHigh = ceil(rowRatio);
        float rowFrac = rowRatio - iLow;

        for (int j = 0; j < INTERPOLATED_SIZE; j++) {
            float colRatio = (float)(j) / (INTERPOLATED_SIZE - 1) * (GRID_SIZE - 1);
            int jLow = floor(colRatio);
            int jHigh = ceil(colRatio);
            float colFrac = colRatio - jLow;

            float top = interpolate(input[iLow][jLow], input[iLow][jHigh], colFrac);
            float bottom = interpolate(input[iHigh][jLow], input[iHigh][jHigh], colFrac);

            output[i][j] = interpolate(top, bottom, rowFrac);
        }
    }
}

void setup() {
    M5.begin();
    M5.Lcd.setRotation(1);  // Adjust rotation if necessary
    M5.Lcd.fillScreen(BLACK);
    
    // Start Serial for debugging
    Serial.begin(115200);
    Serial.println("Starting setup...");

    // Initialize custom I2C pins with a higher clock speed
    Wire.begin(25, 26);  // SDA on GPIO25, SCL on GPIO26
    Wire.setClock(400000);  // Increase I2C clock speed to 400kHz
    Serial.println("Custom I2C pins initialized.");

    // Initialize the AMG8833 sensor
    if (!amg.begin()) {
        M5.Lcd.println("AMG88xx not found!");
        Serial.println("Could not find a valid AMG88xx sensor!");
        while (1) {
            delay(1000);
        }
    } else {
        M5.Lcd.println("AMG88xx Initialized!");
        Serial.println("AMG88xx sensor initialized successfully.");
    }

    // Initialize the last displayed array with invalid values
    for (int i = 0; i < INTERPOLATED_SIZE; i++) {
        for (int j = 0; j < INTERPOLATED_SIZE; j++) {
            lastDisplayed[i][j] = -999.0;
        }
    }

    delay(1000);  // Allow time to read the screen messages
}

void loop() {
    float pixels[AMG88xx_PIXEL_ARRAY_SIZE];
    float image[GRID_SIZE][GRID_SIZE];
    float interpolatedImage[INTERPOLATED_SIZE][INTERPOLATED_SIZE];

    // Read the pixels from the sensor
    amg.readPixels(pixels);

    // Convert the 1D array to a 2D array for easier processing
    for (int i = 0; i < GRID_SIZE; i++) {
        for (int j = 0; j < GRID_SIZE; j++) {
            image[i][j] = pixels[i * GRID_SIZE + j];
        }
    }

    // Find the maximum and minimum temperatures
    float maxTemp = pixels[0];
    float minTemp = pixels[0];
    for (int i = 1; i < AMG88xx_PIXEL_ARRAY_SIZE; i++) {
        if (pixels[i] > maxTemp) {
            maxTemp = pixels[i];
        }
        if (pixels[i] < minTemp) {
            minTemp = pixels[i];
        }
    }

    // Convert temperatures to Fahrenheit
    float maxTempF = maxTemp * 9.0 / 5.0 + 32.0;
    float minTempF = minTemp * 9.0 / 5.0 + 32.0;

    // Interpolate the image to increase resolution
    interpolateImage(image, interpolatedImage);

    // Define the size of each interpolated cell
    int cellWidth = SCREEN_WIDTH / INTERPOLATED_SIZE;
    int cellHeight = SCREEN_HEIGHT / INTERPOLATED_SIZE;

    // Display the interpolated temperature data on the M5StickC screen with auto-ranging colors
    for (int i = 0; i < INTERPOLATED_SIZE; i++) {
        for (int j = 0; j < INTERPOLATED_SIZE; j++) {
            float temp = interpolatedImage[i][j];

            // Check if the temperature has changed significantly before updating the pixel
            if (abs(temp - lastDisplayed[i][j]) > 0.5) {  // Update threshold
                // Auto-range the color mapping
                int color = M5.Lcd.color565(
                    map(temp, minTemp, maxTemp, 0, 255),
                    0,
                    map(temp, minTemp, maxTemp, 255, 0)
                );
                int x = j * cellWidth;
                int y = i * cellHeight;
                M5.Lcd.fillRect(x, y, cellWidth, cellHeight, color);

                // Update the last displayed value
                lastDisplayed[i][j] = temp;
            }
        }
    }

    // Increase the font size by 100%
    M5.Lcd.setTextSize(2);

    // Display the maximum temperature in Fahrenheit at the top left
    M5.Lcd.setTextColor(WHITE, BLACK);
    M5.Lcd.setCursor(0, 0);
    M5.Lcd.printf("%.1fF", maxTempF);

    // Adjust the position of the minimum temperature text to align it to the bottom right corner
    int textWidth = 6 * 2 * 5;  // Approximate width of the text in pixels (6 pixels per character, 2x size, 5 characters "XX.X F")
    M5.Lcd.setCursor(SCREEN_WIDTH - textWidth - 7, SCREEN_HEIGHT - 24);  // Adjust cursor position for larger text
    M5.Lcd.printf("%.1fF", minTempF);

    // Debugging: Print max and min temps to Serial
    Serial.printf("Max Temp: %.1f F, Min Temp: %.1f F\n", maxTempF, minTempF);

    // Check if the big button is pressed
    if (M5.BtnA.wasPressed()) {
        // Beep when the button is pressed
        tone(26, 1000, 100);  // Beep on GPIO26 (adjust if necessary), 1kHz tone, 100ms duration

        // Wait for a moment to allow the beep sound to finish
        delay(100);

        // Power off the device
        M5.Power.powerOff();  // Corrected method name to powerOff()
    }

    // No delay for faster refresh
    M5.update();  // Update the button state
}
