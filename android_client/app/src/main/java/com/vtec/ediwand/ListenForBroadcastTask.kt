package com.vtec.ediwand

import android.util.Log
import androidx.lifecycle.MutableLiveData
import kotlinx.coroutines.*
import java.io.IOException
import java.net.DatagramPacket
import java.net.DatagramSocket
import java.net.InetAddress
import java.util.*

class ListenForBroadcastTask(private val availComputers: MutableLiveData<MutableList<ControlComputer>>, private val port: Int): CoroutineScope
{
    val job = Job()
    override val coroutineContext = job + Dispatchers.IO

    private fun detectedPresence(address: String, data: String)
    {
        val splitData = data.split("|")
        val hostname = splitData.getOrNull(2) ?: "Unknown"
        var os = ControlComputer.OS.Unknown

        try {
            splitData.getOrNull(1)?.let { ControlComputer.OS.valueOf(it) }?.let {
                os = it
            }
        }
        catch (e : IllegalArgumentException) { }

        var add = true
        for (cc in availComputers.value!!) {
            if (cc.address == address && cc.hostname == hostname && cc.os == os)
            {
                cc.recognized = Date()
                add = false
            }
        }
        if (add) {
            availComputers.value!!.add(ControlComputer(hostname, address, os, Date()))
            availComputers.postValue(availComputers.value)
        }
    }

    fun run() = launch {

        var socket: DatagramSocket? = null
        try {
            // Keep a socket open to listen to all the UDP trafic that is destined for this port
            socket = DatagramSocket(port, InetAddress.getByName("0.0.0.0"))
            socket.broadcast = true
            while (!socket.isClosed && isActive) {
                Log.d("UDP", "fds")
                // Receive a packet
                val recvBuf = ByteArray(15000)
                val packet = DatagramPacket(recvBuf, recvBuf.size)
                socket.receive(packet)
                Log.d("UDP", "fds2")

                // Packet received
                val senderAddress = packet.address.hostAddress
                if (senderAddress != null)
                {
                    val data = String(packet.data).trim { it <= ' ' }
                    Log.i("UDP", "Packet received from: $senderAddress")
                    if (data.startsWith("HELLOEDIWAND")) {
                        detectedPresence(senderAddress, data)
                    }
                }
            }
        } catch (ex: IOException) {
            Log.e("UDP", "Oops: " + ex.message)
        } finally {
            if (socket != null && !socket.isClosed) {
                socket.close()
                Log.d("UDP", "SOCKET CLOSED")
            }
        }
    }
}