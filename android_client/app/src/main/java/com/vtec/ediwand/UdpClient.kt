package com.vtec.ediwand

import android.util.Log
import kotlinx.coroutines.*
import java.io.IOException
import java.net.DatagramPacket
import java.net.DatagramSocket
import java.net.InetAddress
import java.net.SocketException

interface UdpClientListener {
    fun messageReceived(msg: String)
    fun connectionClosed()
    fun connectionFailed()
}

class UdpClient(private val listener: UdpClientListener, private val SERVER_IP: String, private val SERVER_PORT: Int) : CoroutineScope {
    private val job = Job()
    override val coroutineContext = job + Dispatchers.IO

    private var socket: DatagramSocket? = null

    fun sendMessage(message: String) = launch {
        socket?.let {
            Log.d("UDP", "Sending: $message")
            it.send(DatagramPacket(message.encodeToByteArray() + "\n".encodeToByteArray(), message.length))
        }
    }

    fun stopClient() = runBlocking {
        job.cancelAndJoin()
    }

    fun run() = launch {
        try {
            socket = DatagramSocket(SERVER_PORT, InetAddress.getByName(SERVER_IP))
            while (socket?.isClosed == false && isActive) {
                // Receive a packet
                val recvBuf = ByteArray(15000)
                val packet = DatagramPacket(recvBuf, recvBuf.size)
                socket?.receive(packet)

                listener.messageReceived(recvBuf.toString())
            }
        } catch (ex: SocketException) {
            Log.d("UDP", "SOCKET CLOSED")
            listener.connectionClosed()
        } catch (ex: IOException) {
            Log.e("UDP", "Oops: ${ex.message}")
            listener.connectionFailed()
        } finally {
            if (socket != null && socket?.isClosed == false) {
                socket?.close()
                Log.d("UDP", "SOCKET CLOSED")
            }
        }
    }
}