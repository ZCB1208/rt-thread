-- led_blink.lua
-- GD32F527V-START PB12 LED 闪烁示例
-- 用法: 在 Lua 交互模式下 dofile("scripts/led_blink.lua")
-- 或直接粘贴下面内容执行

gpio_mode(28, 1)   -- PB12, output (1=PIN_MODE_OUTPUT)

print("LED blinking on PB12...")
for i = 1, 10 do
    gpio_write(28, 1)    -- 亮
    rtos_delay(200)
    gpio_write(28, 0)    -- 灭
    rtos_delay(200)
end
print("Done!")
