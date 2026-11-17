package net.too1.oneui.mobile.audio;

import android.media.AudioFormat;
import android.media.AudioTrack;
import android.os.Handler;
import android.os.HandlerThread;
import android.os.Looper;
import android.os.Process;
import android.os.SystemClock;
import java.nio.ByteBuffer;
import java.util.ArrayDeque;
import java.util.function.Consumer;

/** Bounded PCM-float output. No decoder, remote identity, microphone or file access. */
public final class PcmAudioOutput implements AutoCloseable {
    public static final class Snapshot {
        public final String phase;
        public final long written,played,dropped;
        public final int rate,channels,session;
        public final long generation;
        Snapshot(String phase,long written,long played,long dropped,int rate,int channels,int session,long generation){
            this.phase=phase;this.written=written;this.played=played;this.dropped=dropped;
            this.rate=rate;this.channels=channels;this.session=session;
            this.generation=generation;
        }
    }
    private static final class Packet {
        final int rate,channels;
        final long generation;
        final ByteBuffer bytes;
        int pending;
        Packet(int rate,int channels,long generation,byte[] bytes){
            this.rate=rate;this.channels=channels;this.generation=generation;this.bytes=ByteBuffer.wrap(bytes);
            pending=bytes.length;
        }
    }
    private final Object lock=new Object();
    private final ArrayDeque<Packet> queue=new ArrayDeque<>();
    private final HandlerThread thread=new HandlerThread("OneUI PCM output",Process.THREAD_PRIORITY_AUDIO);
    private final Handler worker;
    private final Handler main=new Handler(Looper.getMainLooper());
    private final Consumer<Snapshot> listener;
    private final Runnable drain=this::drain;
    private volatile Snapshot snapshot=new Snapshot("idle",0,0,0,0,0,0,0);
    private volatile float gain=1;
    private volatile boolean active,closed;
    private long generation,dropped;
    private int queuedBytes;
    private boolean scheduled,discontinuous;
    // These fields belong exclusively to the audio worker.
    private AudioTrack track;
    private int rate,channels;
    private long written,playedBase,reportedAt;
    private long workerGeneration;
    private String lastPhase="";

    public PcmAudioOutput(Consumer<Snapshot> listener){
        this.listener=listener;thread.start();worker=new Handler(thread.getLooper());
    }
    public Snapshot snapshot(){return snapshot;}
    public void gain(float value){if(!Float.isFinite(value))throw new IllegalArgumentException("Finite PCM gain required");gain=Math.max(0,Math.min(1,value));}
    public void start(){
        final long token;
        synchronized(lock){if(closed||active)return;active=true;token=++generation;queue.clear();queuedBytes=0;dropped=0;discontinuous=false;}
        worker.post(()->{synchronized(lock){if(token!=generation)return;}releaseTrack();workerGeneration=token;written=playedBase=0;publish("waiting",true);});
    }
    public void stop(){
        final long token;
        synchronized(lock){if(!active)return;active=false;token=++generation;queue.clear();queuedBytes=0;discontinuous=false;}
        worker.post(()->{synchronized(lock){if(token!=generation)return;}releaseTrack();workerGeneration=token;publish("idle",true);});
    }
    public boolean offer(int rate,int channels,ByteBuffer source,int bytes){
        if(!valid(rate,channels)||bytes<=0||bytes% (channels*4)!=0||bytes>rate*channels*4*120/1000||bytes>source.capacity())return false;
        synchronized(lock){
            if(!active||closed)return false;
            int limit=rate*channels*4/5;
            while(queuedBytes+bytes>limit&&!queue.isEmpty()){
                queuedBytes-=queue.removeFirst().pending;++dropped;discontinuous=true;
            }
            byte[] owned=new byte[bytes];ByteBuffer input=source.duplicate();input.clear();input.limit(bytes);input.get(owned);
            queue.addLast(new Packet(rate,channels,generation,owned));queuedBytes+=bytes;schedule(0);
        }
        return true;
    }
    private static boolean valid(int rate,int channels){
        return (rate==8000||rate==12000||rate==16000||rate==24000||rate==48000)&&(channels==1||channels==2);
    }
    private void schedule(int delay){if(!scheduled){scheduled=true;worker.postDelayed(drain,delay);}}
    private void drain(){
        final long token;
        synchronized(lock){scheduled=false;}
        if(!active||closed){releaseTrack();return;}
        synchronized(lock){if(workerGeneration!=generation)return;token=generation;}
        try {
            for(int i=0;i<8;++i){
                Packet packet;
                boolean reset;
                synchronized(lock){
                    if(token!=generation||!active||closed)return;
                    packet=queue.peekFirst();reset=discontinuous;discontinuous=false;
                }
                if(reset)releaseTrack();
                if(packet==null)break;
                synchronized(lock){if(packet.generation!=generation){if(queue.remove(packet))queuedBytes-=packet.pending;continue;}}
                if(track==null||rate!=packet.rate||channels!=packet.channels)open(packet.rate,packet.channels);
                track.setVolume(gain);
                int count=track.write(packet.bytes,packet.bytes.remaining(),AudioTrack.WRITE_NON_BLOCKING);
                if(count<0)throw new IllegalStateException("PCM output unavailable");
                if(count==0)break;
                written+=count/(channels*4);
                synchronized(lock){if(packet.generation==generation&&queue.contains(packet)){
                    packet.pending-=count;queuedBytes-=count;if(packet.pending==0)queue.remove(packet);}}
            }
            long played=track==null?0:Integer.toUnsignedLong(track.getPlaybackHeadPosition());
            synchronized(lock){if(token!=generation)return;}
            publish(played>0?"playing":"waiting",false);
            synchronized(lock){if(active&&(track!=null||!queue.isEmpty()))schedule(queue.isEmpty()?20:5);}
        }catch(RuntimeException failure){
            boolean obsolete;
            synchronized(lock){
                // A failed write from a previous playback must not cancel a
                // newer start queued while this non-blocking write was running.
                obsolete=token!=generation;
                if(!obsolete){active=false;workerGeneration=++generation;queue.clear();queuedBytes=0;}
            }
            releaseTrack();if(!obsolete)publish("error",true);
        }
    }
    private void open(int rate,int channels){
        releaseTrack();this.rate=rate;this.channels=channels;
        int mask=channels==2?AudioFormat.CHANNEL_OUT_STEREO:AudioFormat.CHANNEL_OUT_MONO;
        int minimum=AudioTrack.getMinBufferSize(rate,mask,AudioFormat.ENCODING_PCM_FLOAT);
        if(minimum<=0)throw new IllegalStateException("Unsupported PCM output");
        track=new AudioTrack.Builder().setAudioAttributes(AudioFocusOwner.attributes())
            .setAudioFormat(new AudioFormat.Builder().setSampleRate(rate).setChannelMask(mask).setEncoding(AudioFormat.ENCODING_PCM_FLOAT).build())
            .setTransferMode(AudioTrack.MODE_STREAM).setBufferSizeInBytes(Math.max(minimum,rate*channels*4/10))
            .setPerformanceMode(AudioTrack.PERFORMANCE_MODE_LOW_LATENCY).build();
        if(track.getState()!=AudioTrack.STATE_INITIALIZED)throw new IllegalStateException("PCM output not initialized");
        track.setVolume(gain);track.play();
    }
    private void releaseTrack(){
        if(track==null)return;
        try{playedBase+=Integer.toUnsignedLong(track.getPlaybackHeadPosition());track.pause();track.flush();}
        catch(IllegalStateException ignored){}
        track.release();track=null;
    }
    private void publish(String phase,boolean force){
        long played=playedBase+(track==null?0:Integer.toUnsignedLong(track.getPlaybackHeadPosition()));
        long losses;synchronized(lock){losses=dropped;}
        Snapshot value=new Snapshot(phase,written,played,losses,rate,channels,track==null?0:track.getAudioSessionId(),workerGeneration);snapshot=value;
        long now=SystemClock.uptimeMillis();
        if(force||!phase.equals(lastPhase)||now-reportedAt>=500){
            lastPhase=phase;reportedAt=now;main.post(()->{synchronized(lock){if(closed||value.generation!=generation)return;}listener.accept(value);});
        }
    }
    @Override public void close(){
        synchronized(lock){if(closed)return;closed=true;active=false;++generation;queue.clear();queuedBytes=0;}
        worker.post(()->{releaseTrack();thread.quitSafely();});
    }
}
