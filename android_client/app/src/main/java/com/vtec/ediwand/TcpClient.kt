package com.vtec.ediwand

import android.util.Log
import kotlinx.coroutines.*
import java.io.*
import java.net.InetAddress
import java.net.Socket
import java.net.SocketException

interface TcpClientListener {
    fun messageReceived(msg: String)
    fun connectionClosed()
    fun connectionFailed()
}

class TcpClient( private val listener: TcpClientListener, private val SERVER_IP: String, private val SERVER_PORT: Int) : CoroutineScope {
    private val job = Job()
    override val coroutineContext = job + Dispatchers.IO

    private lateinit var mBufferOut: PrintWriter

    fun sendMessage(message: String) = launch {
        Log.d("TCP", "Sending: $message")
        mBufferOut.println(message)
        mBufferOut.flush()
    }

    fun stopClient() = runBlocking {
        mBufferOut.flush()
        mBufferOut.close()
        job.cancelAndJoin()
    }

    fun run() = launch {
        var socket: Socket? = null
        try {
            socket = Socket(InetAddress.getByName(SERVER_IP), SERVER_PORT)
            mBufferOut = PrintWriter(BufferedWriter(OutputStreamWriter(socket.getOutputStream())), true)
            val mBufferIn = BufferedReader(InputStreamReader(socket.getInputStream()))
            while (!socket.isClosed && isActive) {
                val mServerMessage = mBufferIn.readLine()
                if (mServerMessage != null) {
                    Log.d("TCP", "Received: $mServerMessage")
                    listener.messageReceived(mServerMessage)
                }
                if (mServerMessage == null) {
                    socket.close()
                    Log.d("TCP", "SOCKET CLOSED")
                    listener.connectionClosed()
                }
            }
        } catch (ex: SocketException) {
            Log.d("TCP", "SOCKET CLOSED")
            listener.connectionClosed()
        } catch (ex: IOException) {
            Log.e("TCP", "Oops: ${ex.message}")
            listener.connectionFailed()
        } finally {
            if (socket != null && !socket.isClosed) {
                socket.close()
                Log.d("TCP", "SOCKET CLOSED")
            }
        }
    }
}