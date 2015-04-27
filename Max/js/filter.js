inlets = 1;
outlets = 1;

var BUFFER_SIZE = 32,
	FACTOR = 20,	// filtering factor, bigger means more filter 0 -100
	MAX_CHANGE = 5;

var output = 0,
 	prev = 0,
	total = 0,
	buffer = [];
	
for (var i = 0; i < BUFFER_SIZE; i++) {
	buffer.push(0);
}

setinletassist(0, "input");
setoutletassist(0, "output");

function msg_int(val) {
	var change = val-prev;
	if (change < 0) change = -change;	// absolute value
	
	if (change > MAX_CHANGE) output = array_avg(val);
	
	prev = val;
	post(output);
	outlet(0, output);
}

function rolling_avg(val) {
	return (FACTOR*output + (100-FACTOR)*val) / 100;
}

function array_avg(val) {
	total -= buffer.shift();
	total += val;
	buffer.push(val);
	var avg = total/BUFFER_SIZE;
	
	return (FACTOR*avg + (100-FACTOR)*val) / 100;
}

function msg_float(val) {

}