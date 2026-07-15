FROM nginx:1.27-alpine

COPY frontend/nginx.conf /etc/nginx/conf.d/default.conf
COPY frontend/index.html frontend/styles.css frontend/trader.css frontend/app.js frontend/scenario.mjs frontend/number_format.mjs frontend/trader.mjs frontend/dispatch_charts.mjs /usr/share/nginx/html/
