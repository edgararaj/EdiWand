package com.vtec.ediwand

import android.hardware.Sensor
import android.hardware.SensorEvent
import android.hardware.SensorEventListener
import android.hardware.SensorManager
import android.os.Bundle
import android.os.Handler
import android.os.Looper
import android.util.Log
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import androidx.fragment.app.Fragment
import com.vtec.ediwand.databinding.FragmentFirstBinding
import java.util.*
import kotlin.concurrent.timerTask

/**
 * A simple [Fragment] subclass as the default destination in the navigation.
 */
class FirstFragment : Fragment(), SensorEventListener {

    private var _binding: FragmentFirstBinding? = null
    private var gyroSensor: Sensor? = null
    private var sensorManager: SensorManager? = null
    private val ballPainter = BallPainter(Handler(Looper.getMainLooper()))

    // This property is only valid between onCreateView and
    // onDestroyView.
    private val binding get() = _binding!!

    override fun onCreateView(
        inflater: LayoutInflater, container: ViewGroup?,
        savedInstanceState: Bundle?
    ): View? {

        _binding = FragmentFirstBinding.inflate(inflater, container, false)
        return binding.root

    }

    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        super.onViewCreated(view, savedInstanceState)

        sensorManager = context?.getSystemService(SensorManager::class.java)
        gyroSensor = sensorManager?.getDefaultSensor(Sensor.TYPE_GYROSCOPE)

        if (gyroSensor != null)
        {
            binding.surface.holder.addCallback(ballPainter)
        }
        else
        {
            Log.e(javaClass.simpleName, "Gyroscope is not supported on this device!")
        }

        /*
        binding.buttonFirst.setOnClickListener {
            findNavController().navigate(R.id.action_FirstFragment_to_SecondFragment)
        }
         */
    }

    private var ppsStartCounter = 0L
    private var pps = 0f
    private lateinit var ppsShowTimer: Timer

    override fun onResume() {
        super.onResume()
        ppsShowTimer = Timer(false)
        ppsShowTimer.schedule(timerTask { Log.d("PPS", "$pps pps on GYRO") }, 0, 1000)
        ppsStartCounter = System.nanoTime()
        sensorManager?.registerListener(this, gyroSensor, SensorManager.SENSOR_DELAY_FASTEST)
        ballPainter.pause(false)
    }

    override fun onPause() {
        super.onPause()
        ppsShowTimer.cancel()
        sensorManager?.unregisterListener(this)
    }

    override fun onDestroyView() {
        super.onDestroyView()
        binding.surface.holder.removeCallback(ballPainter)
        _binding = null
    }

    override fun onSensorChanged(event: SensorEvent) {
        val ppsEndCounter = System.nanoTime()
        pps = 1e9f / (ppsEndCounter - ppsStartCounter)
        ppsStartCounter = ppsEndCounter

        val xRot = - event.values[2]
        val yRot = - event.values[0]
        val zRot = event.values[1]
        ballPainter.x_rot = xRot
        ballPainter.y_rot = yRot
        ballPainter.z_rot = zRot

        ConnectFragment.client.sendMessage("GYRO|$xRot|$yRot|$zRot")
    }

    override fun onAccuracyChanged(p0: Sensor?, p1: Int) {
    }
}