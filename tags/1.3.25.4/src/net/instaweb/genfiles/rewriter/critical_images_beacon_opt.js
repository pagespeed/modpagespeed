(function(){var d=encodeURIComponent,e=window,g=document,h="documentElement",l="length",m="prototype",n="body",p="&",q="&ci=",r=",",s="?",t="img",u="input",v="load",w="on",x="pagespeed_url_hash",y="url=";e.pagespeed=e.pagespeed||{};var z=e.pagespeed,A=function(a,b){this.c=a;this.d=b;this.b=this.e();this.a={}};A[m].e=function(){return{height:e.innerHeight||g[h].clientHeight||g[n].clientHeight,width:e.innerWidth||g[h].clientWidth||g[n].clientWidth}};
A[m].f=function(a){a=a.getBoundingClientRect();return{top:a.top+(void 0!==e.pageYOffset?e.pageYOffset:(g[h]||g[n].parentNode||g[n]).scrollTop),left:a.left+(void 0!==e.pageXOffset?e.pageXOffset:(g[h]||g[n].parentNode||g[n]).scrollLeft)}};A[m].g=function(a){if(0>=a.offsetWidth&&0>=a.offsetHeight)return!1;a=this.f(a);var b=JSON.stringify(a);if(this.a.hasOwnProperty(b))return!1;this.a[b]=!0;return a.top<=this.b.height&&a.left<=this.b.width};
A[m].i=function(){for(var a=[t,u],b={},c=0;c<a[l];++c)for(var f=g.getElementsByTagName(a[c]),k=0;k<f[l];++k)f[k].hasAttribute(x)&&(f[k].getBoundingClientRect&&this.g(f[k]))&&(b[f[k].getAttribute(x)]=!0);b=Object.keys(b);if(0!=b[l]){a=this.c;a+=-1==a.indexOf(s)?s:p;a+=y+d(this.d);a+=q+d(b[0]);for(c=1;c<b[l]&&2E3>a[l];++c)a+=r+d(b[c]);z.criticalImagesBeaconUrl=a;(new Image).src=a}};
z.h=function(a,b,c){if(a.addEventListener)a.addEventListener(b,c,!1);else if(a.attachEvent)a.attachEvent(w+b,c);else{var f=a[w+b];a[w+b]=function(){c.call(this);f&&f.call(this)}}};z.j=function(a,b){var c=new A(a,b);z.h(e,v,function(){e.setTimeout(function(){c.i()},0)})};z.criticalImagesBeaconInit=z.j;})();
