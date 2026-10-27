#!/usr/bin/env python3
"""Real SDL/Wayland game handoff on a private DomeD and API-level OpenXR runtime.
Never connects to the active display/headset or changes the system runtime.
Requires python-xlib, Xvfb and a DomeD build with native XR lease support.
"""
import argparse,json,os,signal,socket,subprocess,sys,tempfile,threading,time,urllib.request
from pathlib import Path
from Xlib import X,XK,display
from Xlib.ext import xtest
signal.pthread_sigmask(signal.SIG_SETMASK,[])
if os.environ.get('TEMPEST_DOME_TEST_BUS')!='1':
    os.execvpe('dbus-run-session',['dbus-run-session','--',sys.executable,*sys.argv],{**os.environ,'TEMPEST_DOME_TEST_BUS':'1'})
root=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser();p.add_argument('--compositor',type=Path,required=True);p.add_argument('--game',type=Path,default=root/'build/tempest-xr');p.add_argument('--runtime',type=Path,default=root/'build/libtempest-mock-openxr.so');a=p.parse_args()
out=root/'artifacts/domed-test';out.mkdir(parents=True,exist_ok=True)
children=[];streams=[];stop=threading.Event();checks=[]
def start(args,env,name,**kwargs):
    log=(out/(name+'.log')).open('w');streams.append(log)
    child=subprocess.Popen([str(v) for v in args],env=env,stdout=log,stderr=log,**kwargs);children.append(child);return child

def wait(fn,seconds=20):
    end=time.monotonic()+seconds
    while time.monotonic()<end:
        try:
            value=fn()
            if value:return value
        except (OSError,ValueError,KeyError,StopIteration):pass
        time.sleep(.05)
    raise AssertionError('Timeout; inspect '+str(out))
def check(condition,label):
    assert condition,label
    checks.append(label);print('PASS '+label,flush=True)
try:
    with tempfile.TemporaryDirectory(prefix='tempest-dome-') as tmp:
        runtime=Path(tmp);runtime.chmod(0o700)
        rd,wr=os.pipe();xenv=os.environ.copy()
        xvfb=start(['Xvfb','-displayfd',str(wr),'-screen','0','1600x1000x24','-nolisten','tcp'],xenv,'xvfb',pass_fds=(wr,));os.close(wr)
        with os.fdopen(rd) as stream:screen=':'+stream.readline().strip()
        with socket.socket() as sock:sock.bind(('127.0.0.1',0));port=sock.getsockname()[1]
        env={**os.environ,'DISPLAY':screen,'XDG_RUNTIME_DIR':tmp,'XDG_CONFIG_HOME':tmp+'/config','XDG_DATA_HOME':tmp+'/data','XDG_STATE_HOME':tmp+'/state','XDG_CACHE_HOME':tmp+'/cache','DOME_LOGIN_SESSION':'0','DOME_START_XR':'0','DOME_APPEARANCE_SERVICE':'0','DOME_PREVIEW_PORT':str(port),'DOME_RENDERER':'gles','LIBGL_ALWAYS_SOFTWARE':'1','SDL_AUDIODRIVER':'dummy','SDL_VIDEODRIVER':'wayland','XDG_CURRENT_DESKTOP':'Dome'}
        for key in ('WAYLAND_DISPLAY','WAYLAND_SOCKET','LD_PRELOAD'):env.pop(key,None)
        for service in ('TRAY','KDECONNECT','KWALLET','POLKIT','IME'):env['DOME_START_'+service]='0'
        compositor=start([a.compositor,'--backend','nested'],env,'compositor')
        http=urllib.request.build_opener(urllib.request.ProxyHandler({}))
        def request(path,body=None):
            req=urllib.request.Request(f'http://127.0.0.1:{port}'+path,data=None if body is None else json.dumps(body).encode(),headers={'Content-Type':'application/json'})
            with http.open(req,timeout=3) as response:return json.load(response)
        def scene():return request('/api/state')['scene']
        def native():return request('/api/native-xr')
        wait(scene)
        contexts=wait(lambda:[(p,json.loads(p.read_text())) for p in runtime.glob('*.dome-session.json')] or None)
        desktop=wait(lambda:next((p.name.removesuffix('.dome-session.json') for p in runtime.glob('*.dome-session.json') if json.loads(p.read_text())['realm']=='desktop'),None))
        xr=wait(lambda:next((p.name.removesuffix('.dome-session.json') for p in runtime.glob('*.dome-session.json') if json.loads(p.read_text())['realm']=='xr'),None))
        def heartbeat():
            while not stop.wait(.1):
                try:request('/api/xr-session',{'id':32471,'visible':True,'focused':True,'head':{'position_m':[0,1.6,0],'orientation_xyzw':[0,0,0,1]},'horizontal_fov_deg':90,'mouse_viewport':[-.7,.7,-.65,.65]})
                except OSError:pass
        thread=threading.Thread(target=heartbeat,daemon=True);thread.start()
        wait(lambda:request('/api/xr-control')['input_realm']=='xr')
        background=start([a.game,'--desktop','--demo','--frames','100000','--no-audio'],{**env,'WAYLAND_DISPLAY':desktop},'background')
        old=wait(lambda:next((w for w in scene()['windows'] if w.get('client_pid')==background.pid and not w['xr_visible']),None));old_id=old['id']
        # Share this fixture window explicitly to represent an existing XR desktop.
        request('/api/action',{'op':'window_visibility','id':old_id,'desktop':True,'xr':True})
        request('/api/action',{'op':'focus','id':old_id});request('/api/action',{'op':'xr_presentation','top':False,'bottom':True,'desktop':True})
        time.sleep(.3);before=scene();old_rect=next(w['desktop'] for w in before['windows'] if w['id']==old_id)
        xd=display.Display(screen);host=wait(lambda:next((w for w in xd.screen().root.query_tree().children if str(w.get_wm_name()or'').startswith('Dome')),None));host.set_input_focus(X.RevertToParent,X.CurrentTime);xd.sync()
        def key(name,pressed=None):
            code=xd.keysym_to_keycode(XK.string_to_keysym(name))
            events=(X.KeyPress,X.KeyRelease) if pressed is None else (X.KeyPress if pressed else X.KeyRelease,)
            for kind in events:xtest.fake_input(xd,kind,code);xd.sync();time.sleep(.07)
        def alt_tab():key('Alt_L',True);key('Tab');key('Alt_L',False)
        def physical_wheel():
            # Deliberately point at the universal bar, away from the game host.
            g=host.get_geometry();xtest.fake_input(xd,X.MotionNotify,x=g.x+12,y=g.y+12);xd.sync();time.sleep(.1)
            for kind in (X.ButtonPress,X.ButtonRelease):xtest.fake_input(xd,kind,4);xd.sync();time.sleep(.08)
        def frames(file):
            return [line for line in file.read_text().splitlines() if line.endswith(' 1')] if file.exists() else []
        def restored():
            s=scene();return s['presentation']==before['presentation'] and s['focused']==old_id and s['active_workspace']==before['active_workspace']
        def launch(realm,name,extra=None):
            report=out/(name+'-report.json');report.unlink(missing_ok=True);log=out/(name+'-frames.log');log.unlink(missing_ok=True)
            child=start([a.game,'--demo','--no-audio','--report',report],{**env,'WAYLAND_DISPLAY':realm,'LD_PRELOAD':str(a.runtime.resolve()),'TEMPEST_TEST_FRAMES':str(log),**(extra or {})},name)
            w=wait(lambda:next((w for w in scene()['windows'] if w.get('client_pid')==child.pid),None))
            return child,w['id'],report,log
        game,gid,report,frame_log=launch(xr,'xr-launch')
        wait(lambda:native()['active'] and native()['owner']==gid)
        wait(lambda:len(frames(frame_log))>=3)
        s=scene();check(not any(s['presentation'].values()),'XR-origin launch enters native OpenXR and hides XR desktop and both bars')
        physical_wheel();alt_tab();wait(restored);wait(lambda:native()['suspended'])
        time.sleep(.3);count=len(frames(frame_log));time.sleep(.4)
        check(len(frames(frame_log))==count,'Alt-Tab stops game XR submissions while restoring the desktop')
        check(next(w['desktop'] for w in scene()['windows'] if w['id']==old_id)==old_rect,'Alt-Tab preserves previous window geometry, workspace, focus and asymmetric bar visibility')
        request('/api/action',{'op':'focus','id':gid});wait(lambda:native()['active']);wait(lambda:len(frames(frame_log))>count)
        key('Return');key('v');wait(lambda:native()['owner'] is None)
        w=next(w for w in scene()['windows'] if w['id']==gid)
        check(w['desktop_visible'] and not w['xr_visible'] and not w['minimized'],'V returns native game to a focused desktop window and releases capture')
        key('Control_L',True);key('q');key('Control_L',False);wait(lambda:game.poll() is not None)
        stats=json.loads(report.read_text());check(stats['launch_xr'] and stats['xr_entries']>=2 and stats['paused_suspends']>=1 and stats['wheel_events']>=1 and stats['gl_error']==0,'Actual game pauses on handoff, resumes XR on focus and receives captured physical wheel input')
        request('/api/action',{'op':'focus','id':old_id});time.sleep(.2)
        game,gid,report,frame_log=launch(desktop,'desktop-launch');time.sleep(.5)
        w=wait(lambda:next((w for w in scene()['windows'] if w['id']==gid and not w['xr_visible']),None))
        check(not native()['active'] and not frames(frame_log) and w['desktop_visible'],'Desktop-origin launch stays on the desktop compositor')
        request('/api/action',{'op':'focus','id':gid});time.sleep(.3);key('v');wait(lambda:native()['active'] and native()['owner']==gid)
        check(len(wait(lambda:frames(frame_log) or None))>0,'V from desktop enters the native XR compositor and captures input')
        game.kill();game.wait();wait(lambda:native()['owner'] is None);wait(restored)
        check(True,'Killed game cannot leave desktop hidden or retain input capture')
        game,gid,report,frame_log=launch(xr,'stalled-client');wait(lambda:native()['active'])
        os.kill(game.pid,signal.SIGSTOP);wait(lambda:native()['owner'] is None,seconds=7);wait(restored)
        check(True,'Expired heartbeat restores the desktop even when all game threads are stopped')
        os.kill(game.pid,signal.SIGKILL);game.wait()
        game,gid,report,frame_log=launch(xr,'runtime-failure',{'TEMPEST_TEST_FAIL_FRAME':'12'})
        wait(lambda:'XR session ended:' in (out/'runtime-failure.log').read_text())
        wait(lambda:native()['owner'] is None)
        w=wait(lambda:next((w for w in scene()['windows'] if w['id']==gid and w['desktop_visible'] and not w['xr_visible']),None))
        check(scene()['presentation']==before['presentation'],'Runtime exception restores bar/desktop presentation and releases the native XR lease')
        request('/api/action',{'op':'close','id':gid});wait(lambda:game.poll() is not None)
        stats=json.loads(report.read_text())
        check(stats['paused'] and not stats['captured'] and not stats['relative_mouse'] and not stats['xr_intent'],'Runtime exception releases SDL capture and pauses the actual game before normal exit')
        request('/api/action',{'op':'focus','id':old_id})
        # A pre-integration DomeD must be rejected before creating an XR session.
        from http.server import BaseHTTPRequestHandler,ThreadingHTTPServer
        class OldDome(BaseHTTPRequestHandler):
            def do_GET(self):self.send_response(404);self.end_headers()
            def log_message(self,*args):pass
        old_server=ThreadingHTTPServer(('127.0.0.1',0),OldDome)
        old_thread=threading.Thread(target=old_server.serve_forever,daemon=True);old_thread.start()
        cookie=runtime/(xr+'.dome-session.json');original=cookie.read_bytes()
        try:
            data=json.loads(original);data['control_port']=old_server.server_port;cookie.write_text(json.dumps(data))
            game,gid,report,frame_log=launch(xr,'old-compositor')
            wait(lambda:'Restart DomeD' in (out/'old-compositor.log').read_text())
            check(not frames(frame_log) and not native()['active'],'Older DomeD leaves desktop visible and rejects XR handoff before creating a native session')
            request('/api/action',{'op':'close','id':gid});wait(lambda:game.poll() is not None)
        finally:
            cookie.write_bytes(original);old_server.shutdown();old_server.server_close();old_thread.join(timeout=2)
        (out/'before.json').write_text(json.dumps(before,indent=2)+'\n');(out/'after.json').write_text(json.dumps(scene(),indent=2)+'\n')
        xd.close()
finally:
    stop.set()
    for child in reversed(children):
        if child.poll() is None:
            child.terminate()
            try:child.wait(timeout=4)
            except subprocess.TimeoutExpired:child.kill();child.wait()
    for stream in streams:stream.close()
    (out/'checks.json').write_text(json.dumps(checks,indent=2)+'\n')
