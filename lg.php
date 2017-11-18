<?php
$n = 101; // Количество уровней яркости
$max_pwm = 1000; // Максимальный уровень PWM
$pwm_lightness_table = array();
for ($x = 1; $x <= $n; ++$x) {
	$y = (1 - log($x, $n));
	$pwm = round($max_pwm * $y);
	
	$pwm_lightness_table[] = $pwm;
	
	echo "$x: ".round($y * 100, 2)."% ".$pwm."\n";
}

echo "\n{".implode(", ", array_reverse($pwm_lightness_table))."}\n";
