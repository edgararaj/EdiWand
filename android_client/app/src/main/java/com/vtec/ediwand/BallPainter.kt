package com.vtec.ediwand

import android.graphics.Canvas
import android.graphics.Color
import android.graphics.Paint
import android.os.Handler
import android.view.SurfaceHolder

class BallPainter(private val handler: Handler) : Runnable, SurfaceHolder.Callback {

    private val paint = Paint().apply {
        color = Color.RED
    }

    private lateinit var surfaceHolder: SurfaceHolder
    private var x = 0f
    private var y = 0f
    private var lastTime = 0L
    private var frameRate = 1000f / 60
    private var width = 0
    private var height = 0
    private var wait = true
    var x_rot = 0f
    var y_rot = 0f
    var z_rot = 0f

    private fun updatePhysics() {
        val currentTime = System.currentTimeMillis()
        if (lastTime > 0)
        {
            y = height / 2f + y_rot * height / 2f
            x = width / 2f + x_rot * width / 2f
        }
        lastTime = currentTime
    }

    /**
     * Mise en pause du thread
     */
    fun pause(paused: Boolean) {
        wait = paused
        // si on est en mode eco et
        // que la pause est supprimee
        // relance du traitement
        if (!wait) {
            handler.postDelayed(this, frameRate.toLong())
        }
    }

    override fun run() {
        var c: Canvas? = null
        updatePhysics()
        try {
            c = surfaceHolder.lockCanvas(null)
            if (c != null) {
                synchronized(surfaceHolder) { onDraw(c) }
            }
        } finally {
            // do this in a finally so that if an exception is thrown
            // during the above, we don't leave the Surface in an
            // inconsistent state
            if (c != null) {
                surfaceHolder.unlockCanvasAndPost(c)
            }
        }
        // lancement du traitement differe en mode eco
        handler.removeCallbacks(this)
        if (!wait)
        {
            handler.postDelayed(this, (frameRate - System.currentTimeMillis() + lastTime).toLong())
        }
    }

    private fun onDraw(canvas: Canvas) {
        canvas.save()

        canvas.drawColor(Color.BLACK)
        canvas.drawCircle(x, y, 5f, paint)

        canvas.restore()
    }

    private fun setSurfaceSize(width: Int, height: Int)
    {
        synchronized(surfaceHolder)
        {
            this.width = width
            this.height = height
            x = width / 2f
            y = height / 2f
        }
    }

    override fun surfaceCreated(holder: SurfaceHolder) {
        surfaceHolder = holder
    }

    override fun surfaceChanged(holder: SurfaceHolder, p1: Int, width: Int, height: Int) {
        setSurfaceSize(width, height)
    }

    override fun surfaceDestroyed(holder: SurfaceHolder) {
        pause(true)
    }
}