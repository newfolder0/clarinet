inlets = 3;
outlets = 2;

var x = 0, 
	y = 0,
	z = 0;
var pitch, roll;

setinletassist(0, "x");
setinletassist(1, "y");
setinletassist(2, "z");
setoutletassist(0, "pitch");
setoutletassist(1, "roll");

function msg_int(val) {
	msg_float(val);
}

function msg_float(val) {
	var g = normalise(val);
	var filter = 1;
	
//	post(g*filter + x*(1-filter));
	
	if (inlet === 0) x = g*filter + x*(1-filter);	
	else if (inlet === 1)	y = g*filter + y*(1-filter);	
	else if (inlet === 2) z = g*filter + z*(1-filter);
	
	doStuff();
}

function normalise(val) {
	var scale = 2;		// scale in g, as set in accelerometer (2 means -2 to 2)
	var range = 4096;	// if input ints are 12-bit, range = 4096
	
	var g = (val/range)*scale*2;
	
	// hack to constrain -1 < g < 1
	if (g > 1) g = 1;
	if (g < -1) g = -1;
	
	return g;
}

function radToDeg(rad) {
	var deg = rad*180/Math.PI;
	return deg;
}

function calcAngles(axisA, axisB) {
	var angleA, angleB, pitch1, pitch2;
	var pi = Math.PI;
	
	angleA = Math.asin(axisA);
	angleB = Math.asin(axisB);
	
	if (angleA >= 0 && angleB >= 0) {
		pitch1 = angleA;
		pitch2 = pi/2 - angleB;
	} else if (angleA >= 0 && angleB < 0) {
		pitch1 = pi - angleA;
		pitch2 = pi/2 - angleB;
	} else if (angleA < 0 && angleB < 0) {
		pitch1 = -pi - angleA;
		pitch2 = angleB - pi/2;		
	} else {
		pitch1 = angleA;
		pitch2 = angleB - pi/2;		
	}
	
	return (pitch1 + pitch2)/2;
}

function doStuff() {
	var pitch, roll;
	var pi = Math.PI;
	
	pitch = calcAngles(-y, z);
	roll = calcAngles(-x, -y);
	
	outlet(0, radToDeg(pitch));
	outlet(1, radToDeg(roll));
}