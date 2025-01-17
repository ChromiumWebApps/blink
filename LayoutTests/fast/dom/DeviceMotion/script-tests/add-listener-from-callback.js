description('Tests that adding a new devicemotion event listener from a callback works as expected.');

var mockAccelerationX = 1.1;
var mockAccelerationY = 2.1;
var mockAccelerationZ = 3.1;

var mockAccelerationIncludingGravityX = 1.2;
var mockAccelerationIncludingGravityY = 2.2;
var mockAccelerationIncludingGravityZ = 3.2;

var mockRotationRateAlpha = 1.3;
var mockRotationRateBeta = 2.3;
var mockRotationRateGamma = 3.3;

var mockInterval = 100;

if (window.testRunner) {
    testRunner.setMockDeviceMotion(true, mockAccelerationX, true, mockAccelerationY, true, mockAccelerationZ,
                                   true, mockAccelerationIncludingGravityX, true, mockAccelerationIncludingGravityY, true, mockAccelerationIncludingGravityZ,
                                   true, mockRotationRateAlpha, true, mockRotationRateBeta, true, mockRotationRateGamma,
                                   mockInterval);
    debug('TEST MODE enabled');
} else
    debug('This test can not be run without the TestRunner');

var deviceMotionEvent;
function checkMotion(event) {
    deviceMotionEvent = event;
    shouldBe('deviceMotionEvent.acceleration.x', 'mockAccelerationX');
    shouldBe('deviceMotionEvent.acceleration.y', 'mockAccelerationY');
    shouldBe('deviceMotionEvent.acceleration.z', 'mockAccelerationZ');

    shouldBe('deviceMotionEvent.accelerationIncludingGravity.x', 'mockAccelerationIncludingGravityX');
    shouldBe('deviceMotionEvent.accelerationIncludingGravity.y', 'mockAccelerationIncludingGravityY');
    shouldBe('deviceMotionEvent.accelerationIncludingGravity.z', 'mockAccelerationIncludingGravityZ');

    shouldBe('deviceMotionEvent.rotationRate.alpha', 'mockRotationRateAlpha');
    shouldBe('deviceMotionEvent.rotationRate.beta', 'mockRotationRateBeta');
    shouldBe('deviceMotionEvent.rotationRate.gamma', 'mockRotationRateGamma');

    shouldBe('deviceMotionEvent.interval', 'mockInterval');
}

function listener(event) {
    checkMotion(event);
    finishJSTest();
}

window.addEventListener('devicemotion', listener);
window.jsTestIsAsync = true;