package com.vtec.ediwand

import android.content.Intent
import android.util.Log
import android.view.LayoutInflater
import android.view.ViewGroup
import androidx.activity.result.ActivityResultLauncher
import androidx.core.content.ContextCompat
import androidx.recyclerview.widget.RecyclerView
import com.vtec.ediwand.databinding.ControlComputerBinding
import java.util.*
import kotlin.concurrent.timerTask

class ControlComputerAdapter(private val computers: List<ControlComputer>, private val launcher: ActivityResultLauncher<Unit>): RecyclerView.Adapter<ControlComputerVH>()
{
    override fun onCreateViewHolder(parent: ViewGroup, viewType: Int): ControlComputerVH {
        val binding = ControlComputerBinding.inflate(LayoutInflater.from(parent.context), parent, false)
        return ControlComputerVH(binding, launcher)
    }

    override fun onBindViewHolder(holder: ControlComputerVH, position: Int) {
        holder.bind(computers[position])
    }

    override fun getItemCount() = computers.size
}

class ControlComputerVH(private val binding: ControlComputerBinding, private val launcher: ActivityResultLauncher<Unit>) : RecyclerView.ViewHolder(binding.root), TcpClientListener
{
    private val context = binding.root.context

    fun bind(computer: ControlComputer)
    {
        binding.root.setOnClickListener {
            ConnectFragment.client = TcpClient(this, computer.address, controlPort)
            ConnectFragment.client.run()
        }
        binding.title.text = computer.hostname
        binding.content.text = computer.address

        binding.osIcon.setImageDrawable(ContextCompat.getDrawable(context, when (computer.os)
        {
            ControlComputer.OS.Linux -> R.drawable.ic_linux
            ControlComputer.OS.MacOS -> R.drawable.ic_mac_os
            ControlComputer.OS.Windows -> R.drawable.ic_windows
            ControlComputer.OS.SerenityOS -> R.drawable.ic_serenity_os
            else -> R.drawable.ic_unknown
        }))
    }

    override fun messageReceived(msg: String) {
        if (msg == "BOTAQUETEM")
        {
            ConnectFragment.clientPingTimer = Timer(false)
            ConnectFragment.clientPingTimer.schedule(timerTask { ConnectFragment.client.sendMessage("PING") }, 0, 2000)

            launcher.launch(Unit)
        }
    }

    override fun connectionClosed() {
        Log.d("TCP", "connectionClosed")
        ConnectFragment.clientPingTimer.cancel()
        context.sendBroadcast(Intent(finishActivityAction))
    }

    override fun connectionFailed() {
        Log.d("TCP", "connectionFailed")
        ConnectFragment.clientPingTimer.cancel()
        context.sendBroadcast(Intent(finishActivityAction))
    }
}